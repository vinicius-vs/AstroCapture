#pragma once

#include <stdexcept>
#include <string>

namespace domain {

class RepositoryError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class RepositoryNotFoundError final : public RepositoryError
{
public:
    explicit RepositoryNotFoundError(const std::string& message)
        : RepositoryError(message)
    {
    }
};

class RepositoryConflictError final : public RepositoryError
{
public:
    explicit RepositoryConflictError(const std::string& message)
        : RepositoryError(message)
    {
    }
};

} // namespace domain
