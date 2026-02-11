#pragma once

#include "duckdb.hpp"
#ifndef DUCKDB_AMALGAMATION
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#endif

namespace duckdb {
class ClientContext;
}

namespace bmtpcds {

struct DSGenWrapper {

    //! Gets the specified TPC-DS Query number as a string
    static std::string GetQuery(int query);
};

} // namespace bmtpcds