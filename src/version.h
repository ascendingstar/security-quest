#pragma once

namespace sq {

inline constexpr const char* kAppVersion = "0.4";

// Формат данных в data\: растёт только при несовместимых изменениях журналов.
// Любая версия программы обязана читать все старые записи; неизвестные типы событий
// пропускаются, поэтому старая программа на новых данных не падает, а лишь не видит новое.
inline constexpr int kDataFormat = 1;

}  // namespace sq
