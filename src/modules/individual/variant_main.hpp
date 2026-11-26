#pragma once
// ============================================================================
// ВАРИАНТ: Система расчёта платежей за жилищно-коммунальные услуги (ЖКУ).
//
// Считает стоимость ХВС, ГВС, электричества и итог.
// ВАЖНО: имя namespace должно остаться TaskVariant — так работает task.hpp.
// ============================================================================

#include <string>
#include <sstream>
#include <iomanip>
#include "../../json.hpp"
#include <cmath>
#include <stdexcept>
#include <cctype>



namespace TaskVariant {

    // ------------------------------------------------------------------------
    // Входные данные.
    // ------------------------------------------------------------------------
    struct Input {
        double volume_hvs;      // объём ХВС (0..1000 м³)
        double tariff_hvs;      // тариф ХВС (1..100 руб/м³)
        double volume_gvs;      // объём ГВС (0..1000 м³)
        double tariff_gvs;      // тариф ГВС (1..200 руб/м³)
    };

    // ------------------------------------------------------------------------
    // Выходные данные — строки, как требует задание.
    // ------------------------------------------------------------------------
    struct Output {
        std::string cost_hvs;             // "ХВС: X руб."
        std::string cost_gvs;             // "ГВС: Y руб."
        std::string cost_electricity;     // "Электричество: Z руб."
        std::string total;                // "Итого: W руб."
    };

    // ------------------------------------------------------------------------
    // Валидация по диапазонам из задания.
    // ------------------------------------------------------------------------
    inline std::string validate(const Input& in) {
        if (!std::isfinite(in.volume_hvs) ||
            !std::isfinite(in.tariff_hvs) ||
            !std::isfinite(in.volume_gvs) ||
            !std::isfinite(in.tariff_gvs))
        { 

            return "All values must be finite numbers"; 

        }

        if (in.volume_hvs < 0.0 || in.volume_hvs > 1000.0)
            return "volume_hvs must be in [0, 1000]";
        if (in.tariff_hvs < 1.0 || in.tariff_hvs > 100.0)
            return "tariff_hvs must be in [1, 100]";
        if (in.volume_gvs < 0.0 || in.volume_gvs > 1000.0)
            return "volume_gvs must be in [0, 1000]";
        if (in.tariff_gvs < 1.0 || in.tariff_gvs > 200.0)
            return "tariff_gvs must be in [1, 200]";
        return "";
    }

    // ------------------------------------------------------------------------
    // Форматирование числа с двумя знаками после запятой (по алгоритму п.5).
    // ------------------------------------------------------------------------
    inline std::string fmt(double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    }

    // ------------------------------------------------------------------------
    // Расчёты (алгоритм из задания).
    // ------------------------------------------------------------------------
    inline Output calculate(const Input& in) {
        Output out{};

        // 1. ХВС = объём × тариф
        double hvs = in.volume_hvs * in.tariff_hvs;

        // 2. ГВС = объём × тариф
        double gvs = in.volume_gvs * in.tariff_gvs;

        // 3. Электричество — фиксированная сумма
        double electricity = 50.0;

        // 4. Итог = сумма
        double total = hvs + gvs + electricity;

        // 5. Форматирование
        out.cost_hvs = "ХВС: " + fmt(hvs) + " руб.";
        out.cost_gvs = "ГВС: " + fmt(gvs) + " руб.";
        out.cost_electricity = "Электричество: " + fmt(electricity) + " руб.";
        out.total = "Итого: " + fmt(total) + " руб.";

        return out;
    }

    // ------------------------------------------------------------------------
    // Разбор JSON-тела запроса.
    // Поддерживаем два формата:
    //   1) именованный: {"volume_hvs":15,"tariff_hvs":5.5, ...}
    //   2) массив:      [15, 5.5, 10, 3.0]
    // ------------------------------------------------------------------------
    inline Input parse(const std::string& body) {
    Input in{};

    // Проверка и преобразование строки в число.
    auto parse_number = [](const std::string& token) -> double {
        size_t pos = 0;
        double value;

        try {
            value = std::stod(token, &pos);
        }
        catch (...) {
            throw std::invalid_argument("Invalid number");
        }

        // Проверяем, что после числа нет лишних символов.
        while (pos < token.size() &&
               std::isspace(static_cast<unsigned char>(token[pos]))) {
            ++pos;
        }

        if (pos != token.size()) {
            throw std::invalid_argument("Invalid number format");
        }

        // Запрещаем NaN и бесконечность.
        if (!std::isfinite(value)) {
            throw std::invalid_argument("Number must be finite");
        }

        return value;
    };

    // ----------------------------------------------------
    // Вариант 1. JSON с именованными полями.
    // ----------------------------------------------------

    if (!body.empty() && body.find('{') != std::string::npos) {

        // Получение значения по имени поля.
        auto get_value = [&](const std::string& key) -> double {

            std::string search_key = "\"" + key + "\"";

            size_t p = body.find(search_key);

            if (p == std::string::npos) {
                throw std::invalid_argument(
                    "Missing field: " + key
                );
            }

            p = body.find(':', p + search_key.size());

            if (p == std::string::npos) {
                throw std::invalid_argument(
                    "Invalid field: " + key
                );
            }

            // Получаем текст после двоеточия.
            size_t start = body.find_first_not_of(" \t\r\n", p + 1);

            if (start == std::string::npos) {
                throw std::invalid_argument(
                    "Empty field: " + key
                );
            }

            size_t end = body.find_first_of(",}\r\n", start);

            std::string token = body.substr(
                start,
                end == std::string::npos
                    ? std::string::npos
                    : end - start
            );

            // Преобразуем значение в число.
            return parse_number(token);
        };

        in.volume_hvs = get_value("volume_hvs");
        in.tariff_hvs = get_value("tariff_hvs");
        in.volume_gvs = get_value("volume_gvs");
        in.tariff_gvs = get_value("tariff_gvs");

        return in;
    }

    // ----------------------------------------------------
    // Вариант 2. JSON-массив из четырёх чисел.
    // ----------------------------------------------------

    size_t lb = body.find('[');
    size_t rb = body.find(']');

    if (lb == std::string::npos ||
        rb == std::string::npos ||
        rb <= lb) {
        throw std::invalid_argument("Invalid JSON input");
    }

    std::string inside = body.substr(
        lb + 1,
        rb - lb - 1
    );

    std::istringstream ss(inside);
    std::string token;
    double vals[4];
    int count = 0;

    while (std::getline(ss, token, ',')) {

        if (count >= 4) {
            throw std::invalid_argument(
                "Array must contain exactly 4 values"
            );
        }

        vals[count] = parse_number(token);
        ++count;
    }

    if (count != 4) {
        throw std::invalid_argument(
            "Array must contain exactly 4 values"
        );
    }

    in.volume_hvs = vals[0];
    in.tariff_hvs = vals[1];
    in.volume_gvs = vals[2];
    in.tariff_gvs = vals[3];

    return in;
}

    // ------------------------------------------------------------------------
    // JSON-ответ. Строки кладём через o.str(...) — то же имя, что для полей,
    // потому что JObj хранит значения как строки JSON (числа уже превращены).
    // ------------------------------------------------------------------------
    inline std::string to_json(const Output& out) {
        JObj o;

        o.str("cost_hvs", out.cost_hvs);
        o.str("cost_gvs", out.cost_gvs);
        o.str("cost_electricity", out.cost_electricity);
        o.str("total", out.total);

        return o.str();
    }
} // namespace TaskVariant