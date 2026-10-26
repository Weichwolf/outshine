#ifndef OUTSHINE_APP_ENGINEFACTORY_H
#define OUTSHINE_APP_ENGINEFACTORY_H

namespace outshine {
class Engine;

namespace App {
[[nodiscard]] Engine CreateEngine();
}
}
#endif
