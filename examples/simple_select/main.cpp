#include <cppq/sql/Select.hpp>
#include <cppq/sql/Insert.hpp>
#include <cppq/sql/Update.hpp>
#include <cppq/sql/Delete.hpp>

#include <iostream>
#include <format>

int main() {
    // SELECT: SELECT id, name, phone FROM users WHERE phone=$1 ORDER BY name ASC LIMIT $2 OFFSET $3
    auto select_q = cppq::select()
        .columns({"id", "name", "phone"})
        .from("users")
        .where(cppq::eq(cppq::col("phone"), cppq::Param(std::string("13800001234"))))
        .order_by("name", cppq::Order::Asc)
        .limit(10)
        .offset(0)
        .build();

    std::cout << "SELECT: " << select_q.sql << "\n";
    std::cout << "  params: ";
    for (const auto& p : select_q.params) {
        std::cout << cppq::param_to_string(p) << " ";
    }
    std::cout << "\n\n";

    // INSERT: INSERT INTO users (name, phone, age) VALUES ($1, $2, $3) RETURNING id
    auto insert_q = cppq::insert()
        .into("users")
        .columns({"name", "phone", "age"})
        .values({cppq::Param(std::string("Alice")),
                 cppq::Param(std::string("13800001234")),
                 cppq::Param(int32_t{25})})
        .returning({"id"})
        .build();

    std::cout << "INSERT: " << insert_q.sql << "\n";
    std::cout << "  params: ";
    for (const auto& p : insert_q.params) {
        std::cout << cppq::param_to_string(p) << " ";
    }
    std::cout << "\n\n";

    // UPDATE: UPDATE users SET name=$1, age=$2 WHERE id=$3
    auto update_q = cppq::update("users")
        .set("name", cppq::Param(std::string("Bob")))
        .set("age", cppq::Param(int32_t{30}))
        .where(cppq::eq(cppq::col("id"), cppq::Param(int64_t{42})))
        .build();

    std::cout << "UPDATE: " << update_q.sql << "\n";
    std::cout << "  params: ";
    for (const auto& p : update_q.params) {
        std::cout << cppq::param_to_string(p) << " ";
    }
    std::cout << "\n\n";

    // DELETE: DELETE FROM users WHERE id=$1
    auto delete_q = cppq::delete_from()
        .from("users")
        .where(cppq::eq(cppq::col("id"), cppq::Param(int64_t{42})))
        .build();

    std::cout << "DELETE: " << delete_q.sql << "\n";
    std::cout << "  params: ";
    for (const auto& p : delete_q.params) {
        std::cout << cppq::param_to_string(p) << " ";
    }
    std::cout << "\n";

    return 0;
}
