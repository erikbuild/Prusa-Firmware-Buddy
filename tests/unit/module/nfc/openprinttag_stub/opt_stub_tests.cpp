#include <catch2/catch.hpp>
#include <test_utils/formatters.hpp>

#include <openprinttag_stub/opt_stub.hpp>
#include <openprinttag/opt_fields.hpp>
#include <openprinttag/opt_reader.hpp>

using namespace openprinttag;
using namespace std;

OPTReader::TagField field(auto f) {
    return { .tag = 0, .section = field_section(f), .field = std::to_underlying(f) };
};

namespace std {
std::ostream &operator<<(std::ostream &os, const OPTReader::WriteReport &report) {
    os << "{existed: " << report.field_existed << "}";
    return os;
}
} // namespace std

TEST_CASE("OPTBackend_Stub", "[openprinttag]") {
    OPTBackend_Stub backend;
    OPTReader reader(backend);

    std::array<char, 128> string_buffer;

    CHECK(reader.read_field_string(field(MainField::material_name), string_buffer) == std::string_view("PLA Prusa Galaxy Black"));
    CHECK(reader.write_field_string(field(MainField::material_name), "PLA Prusa Galaxy Klack") == OPTReader::WriteReport { .field_existed = true, .changed = true });
    CHECK(reader.read_field_string(field(MainField::material_name), string_buffer) == std::string_view("PLA Prusa Galaxy Klack"));

    CHECK(reader.read_field_float(field(AuxField::consumed_weight)) == 100);
}
