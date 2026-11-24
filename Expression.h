#pragma once
#include <memory>
#include <string>

// Wzór funkcji zagęszczenia h(x, y, z, s) wpisywany przez użytkownika.
// Działania: + - * / ^ (potęga), nawiasy; stałe: pi, e;
// funkcje: sin cos tan exp log sqrt abs tanh, min(a,b) max(a,b) pow(a,b).
class Expression {
public:
    bool parse(const std::string& text, std::string& error);
    double eval(double x, double y, double z, double s) const;
    bool valid() const { return root_ != nullptr; }

    struct Node;

private:
    // shared_ptr na stałe drzewo: kopia wyrażenia jest tania i bezpieczna
    // przy jednoczesnym użyciu w kilku wątkach (tylko odczyt).
    std::shared_ptr<const Node> root_;
};
