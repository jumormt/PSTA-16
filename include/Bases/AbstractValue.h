//
// Created by prophe cheng on 2025/1/17.
//

#ifndef Z3_EXAMPLE_ABSTRACTVALUE_H
#define Z3_EXAMPLE_ABSTRACTVALUE_H

#include <type_traits>
#include <string>

namespace SVF
{
/*!
 * Base class of abstract value
 */
class AbstractValue
{
public:
    /// Abstract value kind
    enum AbstractValueK
    {
        IntervalK, ConcreteK, AddressK
    };
private:
    AbstractValueK _kind;

public:
    AbstractValue(AbstractValueK kind) : _kind(kind) {}

    virtual ~AbstractValue() = default;

    AbstractValue(const AbstractValue &) noexcept = default;

    AbstractValue(AbstractValue &&) noexcept = default;

    AbstractValue &operator=(const AbstractValue &) noexcept = default;

    AbstractValue &operator=(AbstractValue &&) noexcept = default;

    inline AbstractValueK getAbstractValueKind() const
    {
        return _kind;
    }

    virtual bool isTop() const = 0;

    virtual bool isBottom() const = 0;
}; // end class AbstractValue
}

#endif
