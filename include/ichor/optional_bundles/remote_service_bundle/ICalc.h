#pragma once

namespace Ichor {
    struct ICalc {
        virtual ~ICalc() = default;

        virtual int add(int a, int b) = 0;
        virtual int subtract(int a, int b) = 0;
        virtual int multiply(int a, int b) = 0;
        virtual int divide(int a, int b) = 0;
    };
}