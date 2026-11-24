#include "Expression.h"
#include <cctype>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <vector>

struct Expression::Node {
    enum Kind { Num, Var, Neg, Add, Sub, Mul, Div, Pow, Func1, Func2 } kind;
    double value = 0.0;
    int var = 0;                         // 0..3 = x, y, z, s
    double (*f1)(double) = nullptr;
    double (*f2)(double, double) = nullptr;
    std::shared_ptr<const Node> a, b;
};

namespace {

using NodeP = std::shared_ptr<const Expression::Node>;
using N = Expression::Node;

NodeP make(N::Kind k, NodeP a = nullptr, NodeP b = nullptr) {
    auto n = std::make_shared<N>();
    n->kind = k;
    n->a = std::move(a);
    n->b = std::move(b);
    return n;
}

double fmin2(double a, double b) { return std::fmin(a, b); }
double fmax2(double a, double b) { return std::fmax(a, b); }
double fpow2(double a, double b) { return std::pow(a, b); }
double fabs1(double a) { return std::fabs(a); }

// Parser rekurencyjny (zstępujący). Poziomy priorytetu:
//   expr  := term (('+'|'-') term)*
//   term  := unary (('*'|'/') unary)*
//   unary := ('-'|'+') unary | power
//   power := primary ('^' unary)?        -> -2^2 = -(2^2), 2^-1 działa
//   primary := liczba | nazwa | nazwa '(' argumenty ')' | '(' expr ')'
class Parser {
public:
    explicit Parser(const std::string& t) : t_(t) {}

    NodeP parseAll() {
        NodeP n = expr();
        skipWs();
        if (i_ < t_.size()) fail("nieoczekiwany znak '" + std::string(1, t_[i_]) + "'");
        return n;
    }

private:
    [[noreturn]] void fail(const std::string& msg) {
        throw std::runtime_error(msg + " (pozycja " + std::to_string(i_ + 1) + ")");
    }
    void skipWs() { while (i_ < t_.size() && std::isspace(static_cast<unsigned char>(t_[i_]))) ++i_; }
    bool accept(char c) {
        skipWs();
        if (i_ < t_.size() && t_[i_] == c) { ++i_; return true; }
        return false;
    }
    void expect(char c) { if (!accept(c)) fail(std::string("oczekiwano '") + c + "'"); }

    NodeP expr() {
        NodeP n = term();
        for (;;) {
            if (accept('+')) n = make(N::Add, n, term());
            else if (accept('-')) n = make(N::Sub, n, term());
            else return n;
        }
    }
    NodeP term() {
        NodeP n = unary();
        for (;;) {
            if (accept('*')) n = make(N::Mul, n, unary());
            else if (accept('/')) n = make(N::Div, n, unary());
            else return n;
        }
    }
    NodeP unary() {
        if (accept('-')) return make(N::Neg, unary());
        if (accept('+')) return unary();
        return power();
    }
    NodeP power() {
        NodeP n = primary();
        if (accept('^')) return make(N::Pow, n, unary());
        return n;
    }
    NodeP primary() {
        skipWs();
        if (i_ >= t_.size()) fail("niekompletny wzór");
        const char c = t_[i_];

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            // from_chars nie zależy od locale (strtod w pl_PL czytałby przecinek).
            double v = 0.0;
            const auto res = std::from_chars(t_.data() + i_, t_.data() + t_.size(), v);
            if (res.ec != std::errc()) fail("niepoprawna liczba");
            i_ = res.ptr - t_.data();
            auto n = std::make_shared<N>();
            n->kind = N::Num;
            n->value = v;
            return n;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const size_t start = i_;
            while (i_ < t_.size() && (std::isalnum(static_cast<unsigned char>(t_[i_])) || t_[i_] == '_')) ++i_;
            const std::string name = t_.substr(start, i_ - start);
            if (accept('(')) return call(name);
            return variable(name);
        }
        if (accept('(')) {
            NodeP n = expr();
            expect(')');
            return n;
        }
        fail("nieoczekiwany znak '" + std::string(1, c) + "'");
    }

    NodeP variable(const std::string& name) {
        static const char* vars[] = {"x", "y", "z", "s"};
        for (int k = 0; k < 4; ++k)
            if (name == vars[k]) {
                auto n = std::make_shared<N>();
                n->kind = N::Var;
                n->var = k;
                return n;
            }
        auto n = std::make_shared<N>();
        n->kind = N::Num;
        if (name == "pi") n->value = M_PI;
        else if (name == "e") n->value = M_E;
        else fail("nieznana zmienna '" + name + "' (dostępne: x, y, z, s, pi, e)");
        return n;
    }

    NodeP call(const std::string& name) {
        struct F1 { const char* name; double (*f)(double); };
        static const F1 one[] = {{"sin", std::sin}, {"cos", std::cos}, {"tan", std::tan},
                                 {"exp", std::exp}, {"log", std::log}, {"sqrt", std::sqrt},
                                 {"abs", fabs1}, {"tanh", std::tanh}};
        struct F2 { const char* name; double (*f)(double, double); };
        static const F2 two[] = {{"min", fmin2}, {"max", fmax2}, {"pow", fpow2}};

        for (const auto& f : one)
            if (name == f.name) {
                NodeP arg = expr();
                expect(')');
                auto n = std::make_shared<N>();
                n->kind = N::Func1;
                n->f1 = f.f;
                n->a = arg;
                return n;
            }
        for (const auto& f : two)
            if (name == f.name) {
                NodeP a = expr();
                expect(',');
                NodeP b = expr();
                expect(')');
                auto n = std::make_shared<N>();
                n->kind = N::Func2;
                n->f2 = f.f;
                n->a = a;
                n->b = b;
                return n;
            }
        fail("nieznana funkcja '" + name + "'");
    }

    const std::string& t_;
    size_t i_ = 0;
};

double evalNode(const N& n, const double v[4]) {
    switch (n.kind) {
        case N::Num:   return n.value;
        case N::Var:   return v[n.var];
        case N::Neg:   return -evalNode(*n.a, v);
        case N::Add:   return evalNode(*n.a, v) + evalNode(*n.b, v);
        case N::Sub:   return evalNode(*n.a, v) - evalNode(*n.b, v);
        case N::Mul:   return evalNode(*n.a, v) * evalNode(*n.b, v);
        case N::Div:   return evalNode(*n.a, v) / evalNode(*n.b, v);
        case N::Pow:   return std::pow(evalNode(*n.a, v), evalNode(*n.b, v));
        case N::Func1: return n.f1(evalNode(*n.a, v));
        case N::Func2: return n.f2(evalNode(*n.a, v), evalNode(*n.b, v));
    }
    return 0.0;
}

}  // namespace

bool Expression::parse(const std::string& text, std::string& error) {
    try {
        Parser p(text);
        root_ = p.parseAll();
        return true;
    } catch (const std::exception& e) {
        root_.reset();
        error = e.what();
        return false;
    }
}

double Expression::eval(double x, double y, double z, double s) const {
    const double v[4] = {x, y, z, s};
    return root_ ? evalNode(*root_, v) : 0.0;
}
