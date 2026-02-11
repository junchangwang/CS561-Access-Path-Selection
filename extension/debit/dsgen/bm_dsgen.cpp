#pragma once

#include "bm_dsgen.hpp"
#include "bmtpcds_constants.hpp"

#ifndef DUCKDB_AMALGAMATION
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/date.hpp"
#include "duckdb/parser/column_definition.hpp"
#include "duckdb/parser/parsed_data/create_table_info.hpp"
#include "duckdb/parser/constraints/not_null_constraint.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/main/appender.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#endif

#define DECLARER /* EXTERN references get defined here */

#include <cassert>
#include <cmath>
#include <mutex>

using namespace duckdb;

namespace bmtpcds {

string DSGenWrapper::GetQuery(int query) {
    if (query <= 0 || query > BMTPCDS_QUERIES_COUNT) {
        throw SyntaxException("Out of range TPC-DS query number %d", query);
    }
    return BMTPCDS_QUERIES[query - 1];
}

} // namespace bmtpcds