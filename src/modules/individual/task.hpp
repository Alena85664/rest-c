#pragma once
// ============================================================================
// МОДУЛЬ INDIVIDUAL: каркас роута.
// ============================================================================

#include <string>
#include "../../lib/httplib.h"
#include "../../database.hpp"
#include "../../json.hpp"
#include "../../routes/auth.hpp"
#include <vector>
#include <utility>
#include <algorithm>

#include "variant_main.hpp"

namespace Individual {
    using namespace std;
    using namespace httplib;

    inline void init_routes(Server& svr) {

        // ─── 1. POST /works/calc-individual ────────────────────────────────────
        svr.Post("/api/v1.1/works/calc-individual", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            if (!uid) { res.status = 401; return; }

            TaskVariant::Input in{};

            try {
                in = TaskVariant::parse(req.body);
            }
            catch (const std::exception& e) {    
                JObj o;
                o.str("error", e.what());
                res.status = 400;
                res.set_content(o.str(), "application/json");
                return;
            }

            string err = TaskVariant::validate(in);
            if (!err.empty()) {
                JObj o; o.str("error", err);
                res.status = 400;
                res.set_content(o.str(), "application/json");
                return;
            }

            auto out = TaskVariant::calculate(in);
            string result = TaskVariant::to_json(out);

            db_run("INSERT INTO works (user_id,datetime,input_json,result_json) "
                   "VALUES (?,?,?,?);",
                   {to_string(uid), now_iso(), req.body, result});
            res.set_content(result, "application/json");
        });

        // ─── 2. GET /works/history-individual ──────────────────────────────────
        svr.Get("/api/v1.1/works/history-individual", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            if (!uid) { res.status = 401; return; }

            int limit = MAX_HISTORY_LIMIT;
            if (req.has_param("limit")) {
                try { limit = std::max(1, std::min(std::stoi(req.get_param_value("limit")), MAX_HISTORY_LIMIT)); }
                catch (...) {}
            }

            JArr arr;
            for (auto& r : db_all(
                "SELECT id,datetime,input_json,result_json FROM works "
                "WHERE user_id=" + to_string(uid) +
                " ORDER BY id DESC LIMIT " + to_string(limit) + ";")) {
                JObj o;
                o.int_("id", stoll(r[0]));
                o.str("datetime", r[1]);
                o.raw("input", r[2]);
                o.raw("result", r[3]);
                arr.add(o.str());
            }
            res.status = 200;
            res.set_content(arr.str(), "application/json"); 
        });

        // ============================================================
        // 3. 20 АВТОТЕСТОВ СИСТЕМЫ РАСЧЁТА ЖКХ
        // ============================================================
        svr.Get("/api/v1.1/admin/run-tests-individual", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            
            if (!uid) {
                res.status = 401;
                return;
            }

            if (!Auth::admin(role)) {
                res.status = 403;
                return;
            }

            using namespace TaskVariant;

            vector<pair<string, bool>> tests;

            // ===== БЛОК 1: исходные тесты (1–10) =====
            Input in1{15, 5.5, 10, 3.0};
            Output out1 = calculate(in1);
            tests.push_back({"1. Стоимость холодной воды", out1.cost_hvs == "ХВС: 82.50 руб."});
            tests.push_back({"2. Стоимость горячей воды", out1.cost_gvs == "ГВС: 30.00 руб."});
            tests.push_back({"3. Фиксированная стоимость электричества", out1.cost_electricity == "Электричество: 50.00 руб."});
            tests.push_back({"4. Общая стоимость первого примера", out1.total == "Итого: 162.50 руб."});

            Input in2{25, 6.0, 15, 4.5};
            Output out2 = calculate(in2);
            tests.push_back({"5. Расчёт второго примера", out2.cost_hvs == "ХВС: 150.00 руб." && out2.cost_gvs == "ГВС: 67.50 руб." && out2.total == "Итого: 267.50 руб."});

            Input in3 = parse("[15, 5.5, 10, 3.0]");
            tests.push_back({"6. Чтение данных из массива", in3.volume_hvs == 15 && in3.tariff_hvs == 5.5 && in3.volume_gvs == 10 && in3.tariff_gvs == 3.0});

            Input in4{0, 1, 0, 1};
            tests.push_back({"7. Проверка нулевого расхода", validate(in4).empty() && calculate(in4).total == "Итого: 50.00 руб."});

            Input in5{1000, 100, 1000, 200};
            tests.push_back({"8. Проверка максимальных значений", validate(in5).empty() && calculate(in5).total == "Итого: 300050.00 руб."});

            Input in6{-1, 5, 10, 3};
            tests.push_back({"9. Отклонение отрицательного объёма", !validate(in6).empty()});

            Input in7{15, 5, 10, 201};
            tests.push_back({"10. Отклонение превышенного тарифа", !validate(in7).empty()});


            // ===== БЛОК 2: новые тесты (11–20) =====

            // 11. Отрицательный тариф ХВС
            Input in8{10, -5, 5, 3};
            tests.push_back({"11. Отклонение отрицательного тарифа ХВС", !validate(in8).empty()});

            // 12. Отрицательный объём ГВС
            Input in9{10, 5, -3, 3};
            tests.push_back({"12. Отклонение отрицательного объёма ГВС", !validate(in9).empty()});

            // 13. Отрицательный тариф ГВС
            Input in10{10, 5, 3, -2};
            tests.push_back({"13. Отклонение отрицательного тарифа ГВС", !validate(in10).empty()});

            // 14. Верхняя граница всех тарифов и объёмов — допустима
            Input in11{1000, 100, 1000, 200};
            tests.push_back({"14. Верхние границы допустимы",
                validate(in11).empty() &&
                calculate(in11).total == "Итого: 300050.00 руб."});

            // 15. Нижние границы тарифов — допустимы
            Input in12{0, 1, 0, 1};
            tests.push_back({"15. Нижние границы тарифов допустимы",
                validate(in12).empty() &&
                calculate(in12).total == "Итого: 50.00 руб."});

            // 16. Дробные значения с точным двоичным представлением
            Input in13{2.5, 4.0, 1.25, 8.0};   // 2.5*4=10.00, 1.25*8=10.00
            Output out13 = calculate(in13);
            tests.push_back({"16. Дробные значения без погрешности",
                out13.cost_hvs == "ХВС: 10.00 руб." &&
                out13.cost_gvs == "ГВС: 10.00 руб." &&
                out13.total    == "Итого: 70.00 руб."});

            // 17. Разбор массива с пробелами
            Input in14 = parse("[ 30 , 7.5 , 20 , 6.0 ]");
            tests.push_back({"17. Чтение массива с пробелами",
                in14.volume_hvs == 30 && in14.tariff_hvs == 7.5 &&
                in14.volume_gvs == 20 && in14.tariff_gvs == 6.0});

            // 18. Сквозной сценарий parse -> validate -> calculate
            Input in15 = parse("[15, 5.5, 10, 3.0]");
            tests.push_back({"18. Сквозной сценарий parse->validate->calculate",
                validate(in15).empty() &&
                calculate(in15).total == "Итого: 162.50 руб."});

            // 19. Электричество фиксировано при валидном входе
            Input in16{0, 1, 0, 1};   // валидно: тарифы в [1..], объёмы 0
            tests.push_back({"19. Электричество не зависит от расхода",
                validate(in16).empty() &&
                calculate(in16).cost_electricity == "Электричество: 50.00 руб."});

            // 20. Сумма компонентов равна итогу (инвариант, без хрупкого substr)
            Input in17{12, 6.0, 8, 4.0};   // 72.00 + 32.00 + 50.00 = 154.00
            Output out17 = calculate(in17);
            tests.push_back({"20. Итог = ХВС + ГВС + Электричество",
                out17.cost_hvs == "ХВС: 72.00 руб." &&
                out17.cost_gvs == "ГВС: 32.00 руб." &&
                out17.total    == "Итого: 154.00 руб."});


            // ===== Формирование отчёта =====
            int passed = 0;
            JArr cases;

            for (size_t i = 0; i < tests.size(); i++) {
                if (tests[i].second) {
                    passed++;
                }
                JObj item;
                item.int_("tc_id", static_cast<int>(i + 1));
                item.str("name", tests[i].first);
                item.str("status", tests[i].second ? "PASSED" : "FAILED");
                cases.add(item.str());
            }

            JObj report;
            report.str("status", passed == static_cast<int>(tests.size()) ? "success" : "failed");
            report.int_("total_tests", static_cast<int>(tests.size()));
            report.int_("passed", passed);
            report.int_("failed", static_cast<int>(tests.size()) - passed);
            report.raw("cases", cases.str());

            std::string json_result = report.str();
            std::cout << "[DEBUG] Размер JSON: " << json_result.size() << " байт" << std::endl;
            std::cout << "[DEBUG] Содержимое JSON: " << json_result << std::endl;

            res.status = 200;
            res.set_content(json_result, "application/json");
        });

    } // Закрываем init_routes
} // Закрываем namespace Individual
