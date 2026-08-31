#include "calc.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

namespace terminadventure::calc
{
    namespace
    {
        class Parser
        {
        public:
            explicit Parser(const std::string& expr) : s_(expr) {}

            bool Run(double& result)
            {
                SkipWs();
                result = Expr();
                SkipWs();
                return ok_ && pos_ == s_.size();
            }

        private:
            const std::string& s_;
            std::size_t pos_ = 0;
            bool ok_ = true;

            bool AtEnd() const { return pos_ >= s_.size(); }
            char Peek() const { return AtEnd() ? '\0' : s_[pos_]; }
            char Next() { return AtEnd() ? '\0' : s_[pos_++]; }

            void SkipWs()
            {
                while (!AtEnd())
                {
                    char c = s_[pos_];
                    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
                    else break;
                }
            }

            bool IsDigitStart() const
            {
                if (AtEnd()) return false;
                char c = s_[pos_];
                return (c >= '0' && c <= '9') || c == '.';
            }

            // expression := term (('+'|'-') term)*
            double Expr()
            {
                double v = Term();
                while (ok_)
                {
                    SkipWs();
                    char c = Peek();
                    if (c == '+') { Next(); v = v + Term(); }
                    else if (c == '-') { Next(); v = v - Term(); }
                    else break;
                }
                return v;
            }

            // term := factor (('*'|'/'|implicit) factor)*
            double Term()
            {
                double v = Factor();
                while (ok_)
                {
                    SkipWs();
                    char c = Peek();
                    if (c == '*') { Next(); v = v * Factor(); }
                    else if (c == '/')
                    {
                        Next();
                        double d = Factor();
                        if (std::fabs(d) < 1e-15) { ok_ = false; return 0.0; }
                        v = v / d;
                    }
                    else if (c == '(' || IsDigitStart())
                    {
                        // Implicit multiplication: "2(1+3)" or "4(2)".
                        v = v * Factor();
                    }
                    else break;
                }
                return v;
            }

            // factor := ('-') factor | primary
            double Factor()
            {
                SkipWs();
                if (Peek() == '-')
                {
                    Next();
                    return -Factor();
                }
                return Primary();
            }

            // primary := number | '(' expression ')'
            double Primary()
            {
                SkipWs();
                if (ok_ && Peek() == '(')
                {
                    Next();
                    double v = Expr();
                    SkipWs();
                    if (ok_ && Peek() == ')') { Next(); return v; }
                    ok_ = false;
                    return 0.0;
                }
                if (IsDigitStart())
                {
                    double v;
                    if (!ParseNumber(v)) { ok_ = false; return 0.0; }
                    return v;
                }
                ok_ = false;
                return 0.0;
            }

            bool ParseNumber(double& out)
            {
                std::string num;
                bool dot = false;
                while (!AtEnd())
                {
                    char c = Peek();
                    if (c >= '0' && c <= '9')
                    {
                        num += c;
                        Next();
                    }
                    else if (c == '.' && !dot)
                    {
                        dot = true;
                        num += c;
                        Next();
                    }
                    else break;
                }
                if (num.empty()) return false;
                char* end = nullptr;
                out = std::strtod(num.c_str(), &end);
                return end != num.c_str();
            }
        };
    }

    bool Evaluate(const std::string& expr, std::string& out)
    {
        if (expr.empty()) return false;
        Parser parser(expr);
        double v = 0.0;
        if (!parser.Run(v)) return false;
        if (!std::isfinite(v)) return false;

        // Format the result as an integer when it is one, else as a decimal
        // with trailing zeros trimmed.
        const double rounded = std::round(v);
        if (std::fabs(v - rounded) < 1e-9 && std::fabs(rounded) < 1e15)
        {
            long long ll = static_cast<long long>(rounded);
            out = std::to_string(ll);
            return true;
        }
        std::string s = std::to_string(v);
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
        out = s;
        return true;
    }
}
