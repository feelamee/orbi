#include <orbi/detail/util.hpp>
#include <orbi/semver.hpp>

namespace orbi
{

std::optional<semver>
semver::parse(std::string_view /*s*/)
{
    return std::nullopt;
}

semver::version
semver::major() const
{
    return major_;
}

semver::version
semver::minor() const
{
    return minor_;
}

semver::version
semver::patch() const
{
    return patch_;
}

} // namespace orbi
