//
// Created by prophe cheng on 2025/1/17.
//

#ifndef PSTA_BOUNDEDZ3EXPR_H
#define PSTA_BOUNDEDZ3EXPR_H

#define MaxBvLen 64

#include "Util/Z3Expr.h"
#include "Util/Options.h"

namespace SVF
{

/*!
 * Atom Z3 expr for unlimited precision integers
 */
class BoundedZ3Expr : public Z3Expr
{

public:
    BoundedZ3Expr() = default;

    BoundedZ3Expr(const Z3Expr &z3Expr) : Z3Expr(z3Expr) {}

    BoundedZ3Expr(const z3::expr &e) : Z3Expr(e) {}

    BoundedZ3Expr(s32_t i) : Z3Expr(i) {}

    BoundedZ3Expr(s64_t i) : Z3Expr(getContext().int_val((int64_t)i)) {}

    BoundedZ3Expr(const BoundedZ3Expr &z3Expr) : Z3Expr(z3Expr) {}

    inline BoundedZ3Expr &operator=(const BoundedZ3Expr &rhs)
    {
        Z3Expr::operator=(rhs);
        return *this;
    }

    BoundedZ3Expr(BoundedZ3Expr &&z3Expr) : Z3Expr(z3Expr) {}


    inline BoundedZ3Expr &operator=(BoundedZ3Expr &&rhs)
    {
        Z3Expr::operator=(rhs);
        return *this;
    }

    bool is_plus_infinite() const
    {
        return eq(*this, getContext().int_const("+oo"));
    }

    bool is_minus_infinite() const
    {
        return eq(*this, getContext().int_const("-oo"));
    }

    bool is_infinite() const
    {
        return is_plus_infinite() || is_minus_infinite();
    }

    void set_plus_infinite()
    {
        *this = plus_infinity();
    }

    void set_minus_infinite()
    {
        *this = minus_infinity();
    }

    static BoundedZ3Expr plus_infinity()
    {
        return getContext().int_const("+oo");
    }

    static BoundedZ3Expr minus_infinity()
    {
        return getContext().int_const("-oo");
    }

    static z3::context &getContext()
    {
        return Z3Expr::getContext();
    }

    bool is_zero() const
    {
        return getExpr().is_numeral() && eq(getExpr(), Z3Expr(0));
    }

    static bool isZero(const BoundedZ3Expr &expr)
    {
        return expr.is_numeral() && eq(expr.getExpr(), Z3Expr(0));
    }

    BoundedZ3Expr equal(const BoundedZ3Expr &rhs) const
    {
        return getExpr() == rhs.getExpr();
    }

    BoundedZ3Expr leq(const BoundedZ3Expr &rhs) const
    {
        return getExpr() <= rhs.getExpr();
    }

    BoundedZ3Expr geq(const BoundedZ3Expr &rhs) const
    {
        return getExpr() >= rhs.getExpr();
    }

    /// Reload operator
    //{%
    friend BoundedZ3Expr operator==(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return lhs.equal(rhs);
    }

    friend BoundedZ3Expr operator!=(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return !lhs.equal(rhs);
    }

    friend BoundedZ3Expr operator>(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return !lhs.leq(rhs);
    }

    friend BoundedZ3Expr operator<(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return !lhs.geq(rhs);
    }

    friend BoundedZ3Expr operator<=(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return lhs.leq(rhs);
    }

    friend BoundedZ3Expr operator>=(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return lhs.geq(rhs);
    }

    friend BoundedZ3Expr operator+(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        if (!lhs.is_infinite() && !rhs.is_infinite())
            return lhs.getExpr() + rhs.getExpr();
        else if (!lhs.is_infinite() && rhs.is_infinite())
            return rhs;
        else if (lhs.is_infinite() && !rhs.is_infinite())
            return lhs;
        else if (eq(lhs, rhs))
            return lhs;
        else
            assert(false && "undefined operation +oo + -oo");
    }

    friend BoundedZ3Expr operator-(const BoundedZ3Expr &lhs)
    {
        return -lhs.getExpr();
    }

    friend BoundedZ3Expr operator-(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        if (!lhs.is_infinite() && !rhs.is_infinite())
            return lhs.getExpr() - rhs.getExpr();
        else if (!lhs.is_infinite() && rhs.is_infinite())
            return -rhs;
        else if (lhs.is_infinite() && !rhs.is_infinite())
            return lhs;
        else if (!eq(lhs, rhs))
            return lhs;
        else
            assert(false && "undefined operation +oo - +oo");
    }

    friend BoundedZ3Expr operator*(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        if (lhs.is_zero() || rhs.is_zero()) return 0;
        else if (lhs.is_infinite() && rhs.is_infinite())
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
        else if (lhs.is_infinite())
            return ite(rhs.getExpr() > 0, lhs, -lhs);
        else if (rhs.is_infinite())
            return ite(lhs.getExpr() > 0, rhs, -rhs);
        else
            return lhs.getExpr() * rhs.getExpr();
    }

    friend BoundedZ3Expr operator/(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        if (rhs.is_zero()) assert(false && "divide by zero");
        else if (!lhs.is_infinite() && !rhs.is_infinite())
            return lhs.getExpr() / rhs.getExpr();
        else if (!lhs.is_infinite() && rhs.is_infinite())
            return 0;
        else if (lhs.is_infinite() && !rhs.is_infinite())
            return ite(rhs.getExpr() > 0, lhs, -lhs);
        else
            // TODO: +oo/-oo L'Hôpital's rule?
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
    }

    friend BoundedZ3Expr operator%(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        if (rhs.is_zero()) assert(false && "divide by zero");
        else if (!lhs.is_infinite() && !rhs.is_infinite())
            return lhs.getExpr() % rhs.getExpr();
        else if (!lhs.is_infinite() && rhs.is_infinite())
            return 0;
            // TODO: not sure
        else if (lhs.is_infinite() && !rhs.is_infinite())
            return ite(rhs.getExpr() > 0, lhs, -lhs);
        else
            // TODO: +oo/-oo L'Hôpital's rule?
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
    }

    friend BoundedZ3Expr operator^(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return bv2int(int2bv(MaxBvLen, lhs.getExpr()) ^ int2bv(MaxBvLen, rhs.getExpr()), true);
    }

    friend BoundedZ3Expr operator&(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return bv2int(int2bv(MaxBvLen, lhs.getExpr()) & int2bv(MaxBvLen, rhs.getExpr()), true);
    }

    friend BoundedZ3Expr operator|(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return bv2int(int2bv(MaxBvLen, lhs.getExpr()) | int2bv(MaxBvLen, rhs.getExpr()), true);
    }

    friend BoundedZ3Expr ashr(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinite())
            return lhs;
        else if (rhs.is_infinite())
            return ite(lhs.getExpr() >= 0, BoundedZ3Expr(0), BoundedZ3Expr(-1));
        else
        {
            return bv2int(ashr(int2bv(MaxBvLen, lhs.getExpr()), int2bv(MaxBvLen, rhs.getExpr())), true);
        }
    }

    friend BoundedZ3Expr shl(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinite())
            return lhs;
        else if (rhs.is_infinite())
            return ite(lhs.getExpr() >= 0, plus_infinity(), minus_infinity());
        else
        {
            return bv2int(shl(int2bv(MaxBvLen, lhs.getExpr()), int2bv(MaxBvLen, rhs.getExpr())), true);
        }
    }

    friend BoundedZ3Expr lshr(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return bv2int(lshr(int2bv(MaxBvLen, lhs.getExpr()), int2bv(MaxBvLen, rhs.getExpr())), true);
    }

    friend BoundedZ3Expr int2bv(u32_t n, const BoundedZ3Expr &e)
    {
        return int2bv(n, e.getExpr());
    }

    friend BoundedZ3Expr bv2int(const BoundedZ3Expr &e, bool isSigned)
    {
        return bv2int(e.getExpr(), isSigned);
    }

    friend BoundedZ3Expr operator&&(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return lhs.getExpr() && rhs.getExpr();
    }

    friend BoundedZ3Expr operator||(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return lhs.getExpr() || rhs.getExpr();
    }

    friend BoundedZ3Expr operator!(const BoundedZ3Expr &lhs)
    {
        return !lhs.getExpr();
    }

    friend BoundedZ3Expr ite(const BoundedZ3Expr &cond, const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return ite(cond.getExpr(), lhs.getExpr(), rhs.getExpr());
    }

    friend std::ostream &operator<<(std::ostream &out, const BoundedZ3Expr &expr)
    {
        out << expr.getExpr();
        return out;
    }

    friend bool eq(const BoundedZ3Expr &lhs, const BoundedZ3Expr &rhs)
    {
        return eq(lhs.getExpr(), rhs.getExpr());
    }

    inline BoundedZ3Expr simplify() const
    {
        return getExpr().simplify();
    }

    inline bool is_true() const
    {
        return getExpr().is_true();
    }

    /// Return Numeral
    inline s64_t getNumeral() const
    {
        if (is_numeral())
        {
            int64_t i;
            if (getExpr().is_numeral_i64(i))
                return (s64_t)get_numeral_int64();
            else
            {
                return (getExpr() < 0).simplify().is_true() ? INT64_MIN : INT64_MAX;
            }
        }
        if (is_minus_infinite())
        {
            return INT64_MIN;
        }
        else if (is_plus_infinite())
        {
            return INT64_MAX;
        }
        else
        {
            assert(false && "other literal?");
        }
    }

    s64_t bvLen() const const {
        if(is_infinite()) return (s64_t)Options::MaxBVLen();
        // No overflow
        if(getNumeral() != INT64_MIN && getNumeral() != INT64_MAX) return (s64_t)Options::MaxBVLen();
        // Create a symbolic variable
        Z3Expr x = getContext().real_const("x");
        Z3Expr y = getContext().real_const("y");

        // Add constraints and assertions
        Z3Expr constraint1 = x > 0;                          // x > 0
        Z3Expr constraint2 = x == z3::pw(2, y.getExpr()); // x = 2^y, where y is a real variable
        Z3Expr assertions = constraint1 && constraint2;
        Z3Expr::getSolver().push();
        // Add assertions to the solver
        Z3Expr::getSolver().add(assertions.getExpr());

        // Check for a solution
        if (solver->check() == z3::sat)
        {
            z3::model model = solver->get_model();
            Z3Expr log2_x = model.eval(y.getExpr(), true);
            Z3Expr::getSolver().pop();
            return (s64_t)BoundedZ3Expr(log2_x + 1).simplify().getNumeral();
        }
        else
        {
            Z3Expr::getSolver().pop();
            return (s64_t)Options::MaxBVLen();
        }
    }
    //%}
}; // end class ConZ3Expr
} // end namespace SVF

/// Specialise hash for ConZ3Expr
template<>
struct std::hash<SVF::BoundedZ3Expr>
{
    size_t operator()(const SVF::BoundedZ3Expr &z3Expr) const
    {
        return z3Expr.hash();
    }
};

#endif //PSTA_BOUNDEDZ3EXPR_H
