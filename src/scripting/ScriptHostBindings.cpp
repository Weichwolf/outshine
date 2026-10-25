#include "ScriptRuntime.h"
#include "Script.h"
#include <quickjs.h>
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Script {
namespace {
struct ReferenceId {
  int Id = 0;
};

Runtime &State(JSContext *context) {
  return *static_cast<Runtime *>(JS_GetContextOpaque(context));
}

Value HostObject(JSContext *context, JSValueConst object) {
  const auto &state = State(context);
  const auto *reference = static_cast<ReferenceId *>(JS_GetOpaque(object, state.ReferenceClass));
  return reference != nullptr ? Value::OfRef(reference->Id) : Value{};
}

void FinalizeReference(JSRuntime *vm, JSValue value) {
  js_free_rt(vm, JS_GetOpaque(value, JS_GetClassID(value)));
}

JSValue Inherited(JSContext *context, JSValueConst object, JSAtom atom) {
  const JSValue prototype = JS_GetPrototype(context, object);
  if (JS_IsException(prototype)) { return prototype; }
  const JSValue value =
      JS_IsNull(prototype) ? JS_UNDEFINED : JS_GetProperty(context, prototype, atom);
  JS_FreeValue(context, prototype);
  return value;
}

JSValue Property(JSContext *context,
                 JSValueConst object,
                 JSAtom atom,
                 [[maybe_unused]] JSValueConst receiver) {
  Runtime &state = State(context);
  if (state.Bindings == nullptr) { return JS_UNDEFINED; }
  const JSValue key = JS_AtomToValue(context, atom);
  if (JS_IsException(key)) { return key; }
  const bool symbol = JS_IsSymbol(key);
  JS_FreeValue(context, key);
  if (symbol) { return Inherited(context, object, atom); }
  if (!state.UseHost()) { return JS_EXCEPTION; }
  size_t nameBytes = 0;
  const char *name = JS_AtomToCStringLen(context, &nameBytes, atom);
  if (name == nullptr) { return JS_EXCEPTION; }
  if (nameBytes > state.Limit.HostBytes) {
    JS_FreeCString(context, name);
    return JS_ThrowTypeError(context, "host property name exceeds its byte budget");
  }
  const Value holder = HostObject(context, object);
  const std::string_view property(name, nameBytes);
  const Value value = holder.What == Kind::Ref ? state.Bindings->Member(holder, property)
                                               : state.Bindings->Global(property);
  JS_FreeCString(context, name);
  if (value.What == Kind::Nothing) { return Inherited(context, object, atom); }
  return state.ToJs(value);
}

int HasProperty(JSContext *context, JSValueConst object, JSAtom atom) {
  const JSValue value = Property(context, object, atom, object);
  if (JS_IsException(value)) { return -1; }
  const int held = JS_IsUndefined(value) ? 0 : 1;
  JS_FreeValue(context, value);
  return held;
}

int SetProperty(JSContext *context,
                JSValueConst object,
                JSAtom atom,
                JSValueConst raw,
                JSValueConst receiver,
                [[maybe_unused]] int flags) {
  Runtime &state = State(context);
  if (HostObject(context, object).What != Kind::Ref) {
    return JS_DefinePropertyValue(
        context, receiver, atom, JS_DupValue(context, raw), JS_PROP_C_W_E);
  }
  const JSValue key = JS_AtomToValue(context, atom);
  if (JS_IsException(key)) { return -1; }
  const bool symbol = JS_IsSymbol(key);
  JS_FreeValue(context, key);
  if (symbol) {
    (void)JS_ThrowTypeError(context, "host property names require strings");
    return -1;
  }
  if (!state.UseHost()) { return -1; }
  const auto value = state.FromJs(raw, state.Limit.HostBytes);
  if (state.Bindings == nullptr || !value) {
    (void)JS_ThrowTypeError(context, "unsupported host property value");
    return -1;
  }
  size_t nameBytes = 0;
  const char *name = JS_AtomToCStringLen(context, &nameBytes, atom);
  if (name == nullptr) { return -1; }
  if (nameBytes > state.Limit.HostBytes) {
    JS_FreeCString(context, name);
    (void)JS_ThrowTypeError(context, "host property name exceeds its byte budget");
    return -1;
  }
  const bool accepted = state.Bindings->SetMember(
      HostObject(context, object), std::string_view(name, nameBytes), *value);
  JS_FreeCString(context, name);
  if (!accepted) { (void)JS_ThrowTypeError(context, "host rejected property assignment"); }
  return accepted ? 1 : -1;
}

JSValue Call(JSContext *context,
             const JSValueConst function,
             [[maybe_unused]] JSValueConst receiver,
             int count,
             JSValueConst *arguments,
             [[maybe_unused]] int flags) {
  Runtime &state = State(context);
  if (state.Bindings == nullptr || count < 0 || std::cmp_greater(count, kMaxArgs)) {
    return JS_ThrowTypeError(context, "host call exceeds its argument contract");
  }
  if (!state.UseHost()) { return JS_EXCEPTION; }
  std::array<Value, kMaxArgs> values;
  size_t remaining = state.Limit.HostBytes;
  for (int argument = 0; argument < count; ++argument) {
    auto value = state.FromJs(arguments[argument], remaining);
    if (!value) { return JS_ThrowTypeError(context, "%s", value.error().c_str()); }
    remaining -= value->Text.size();
    values[static_cast<size_t>(argument)] = std::move(*value);
  }
  Value result;
  if (!state.Bindings->Call(HostObject(context, function),
                            std::span(values).first(static_cast<size_t>(count)),
                            result)) {
    return JS_ThrowTypeError(context, "host rejected call");
  }
  return state.ToJs(result);
}
}

bool Runtime::BindHost(std::string &error) {
  static JSClassExoticMethods properties = [] {
    JSClassExoticMethods methods{};
    methods.get_property = Property;
    methods.has_property = HasProperty;
    methods.set_property = SetProperty;
    return methods;
  }();
  JS_NewClassID(Vm, &ReferenceClass);
  JSClassDef references{};
  references.class_name = "OutshineHostReference";
  references.finalizer = FinalizeReference;
  references.call = Call;
  references.exotic = &properties;
  JSClassID globals = 0;
  JS_NewClassID(Vm, &globals);
  JSClassDef globalProperties{};
  globalProperties.class_name = "OutshineHostGlobals";
  globalProperties.exotic = &properties;
  if (JS_NewClass(Vm, ReferenceClass, &references) < 0 ||
      JS_NewClass(Vm, globals, &globalProperties) < 0) {
    error = "JavaScript host class registration failed";
    return false;
  }
  const JSValue global = JS_GetGlobalObject(Context);
  const JSValue hostGlobals = JS_NewObjectClass(Context, globals);
  const JSValue previousPrototype = JS_GetPrototype(Context, global);
  const JSValue function = JS_GetPropertyStr(Context, global, "Function");
  const JSValue functionPrototype = JS_GetPropertyStr(Context, function, "prototype");
  const bool valid = !JS_IsException(previousPrototype) && !JS_IsException(function) &&
                     !JS_IsException(functionPrototype);
  if (valid) {
    JS_SetClassProto(Context, ReferenceClass, functionPrototype);
  } else {
    JS_FreeValue(Context, functionPrototype);
  }
  const bool bound = !JS_IsException(global) && !JS_IsException(hostGlobals) && valid &&
                     JS_SetPrototype(Context, hostGlobals, previousPrototype) >= 0 &&
                     JS_SetPrototype(Context, global, hostGlobals) >= 0;
  JS_FreeValue(Context, function);
  JS_FreeValue(Context, previousPrototype);
  JS_FreeValue(Context, hostGlobals);
  JS_FreeValue(Context, global);
  if (!bound) { error = Exception(); }
  return bound;
}

JSValue Runtime::ToJs(const Value &value) {
  switch (value.What) {
    case Kind::Nothing: return JS_UNDEFINED;
    case Kind::Null: return JS_NULL;
    case Kind::Number: return JS_NewFloat64(Context, value.Number);
    case Kind::Boolean: return JS_NewBool(Context, value.Truth());
    case Kind::Text:
      return value.Text.size() <= Limit.HostBytes
                 ? JS_NewStringLen(Context, value.Text.data(), value.Text.size())
                 : JS_ThrowTypeError(Context, "host result exceeds its byte budget");
    case Kind::Ref: break;
  }
  for (size_t row = 0; row < ReferenceCount; ++row) {
    if (References[row].Id == value.Ref) { return JS_DupValue(Context, References[row].Object); }
  }
  if (ReferenceCount == References.size()) {
    return JS_ThrowInternalError(Context, "JavaScript host reference budget exhausted");
  }
  JSValue object = JS_NewObjectClass(Context, ReferenceClass);
  if (JS_IsException(object)) { return object; }
  auto *reference = static_cast<ReferenceId *>(js_mallocz(Context, sizeof(ReferenceId)));
  if (reference == nullptr) {
    JS_FreeValue(Context, object);
    return JS_ThrowOutOfMemory(Context);
  }
  reference->Id = value.Ref;
  (void)JS_SetOpaque(object, reference);
  References[ReferenceCount++] = {.Id = value.Ref, .Object = JS_DupValue(Context, object)};
  return object;
}

std::expected<Value, std::string> Runtime::FromJs(JSValueConst value, size_t availableBytes) const {
  if (JS_IsUndefined(value)) { return Value{}; }
  if (JS_IsNull(value)) { return Value::OfNull(); }
  if (JS_IsBool(value)) { return Value::OfBoolean(JS_ToBool(Context, value) != 0); }
  if (JS_IsNumber(value)) {
    double number = 0.0;
    if (JS_ToFloat64(Context, &number, value) >= 0) { return Value::OfNumber(number); }
    return std::unexpected("could not convert JavaScript number");
  }
  if (JS_IsString(value)) {
    size_t bytes = 0;
    const char *text = JS_ToCStringLen(Context, &bytes, value);
    if (text == nullptr) { return std::unexpected("could not convert JavaScript string"); }
    if (bytes > availableBytes) {
      JS_FreeCString(Context, text);
      return std::unexpected("host argument exceeds its byte budget");
    }
    Value result = Value::OfText(std::string(text, bytes));
    JS_FreeCString(Context, text);
    return result;
  }
  const auto *reference = static_cast<ReferenceId *>(JS_GetOpaque(value, ReferenceClass));
  if (reference != nullptr) { return Value::OfRef(reference->Id); }
  return std::unexpected("host arguments require primitive values or native host references");
}

}
