#pragma once

#include <QString>

#include <cstdint>
#include <vector>

namespace infrastructure {

struct InspectedSessionFile
{
    QString absolutePath;
    std::uint64_t size;
    bool temporary;
};

class SessionFileInspector
{
public:
    virtual ~SessionFileInspector() = default;

    virtual std::vector<InspectedSessionFile> inspect(const QString& sessionDirectory) const = 0;
};

class LocalSessionFileInspector final : public SessionFileInspector
{
public:
    std::vector<InspectedSessionFile> inspect(const QString& sessionDirectory) const override;
};

} // namespace infrastructure
