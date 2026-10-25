#ifndef OUTSHINE_ENGINE_INPUT_UIACTION_H
#define OUTSHINE_ENGINE_INPUT_UIACTION_H

#include <expected>
#include <string>
#include <string_view>

namespace outshine {
class Host;

[[nodiscard]] std::expected<bool, std::string>
RunUiAction(Host *receiver, std::string_view programme, std::string_view action);

}
#endif
