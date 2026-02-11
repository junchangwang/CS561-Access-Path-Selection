#include "duckdb/execution/execution_context.hpp"
#include "duckdb/main/client_context.hpp"
#include "execution/tpch/bitmap_table_scan.hpp"
#include "bitmaps/rabit/table.h"
#include "duckdb/common/enums/operator_result_type.hpp"
#include "duckdb/storage/table/scan_state.hpp"
#include "duckdb/function/table/table_scan.hpp"
#include "duckdb/transaction/duck_transaction.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <netinet/in.h>

namespace duckdb {

class TableScanGlobalSourceState : public GlobalSourceState {
public:
	TableScanGlobalSourceState(ClientContext &context, const PhysicalTableScan &op) {
		if (op.dynamic_filters && op.dynamic_filters->HasFilters()) {
			table_filters = op.dynamic_filters->GetFinalTableFilters(op, op.table_filters.get());
		}

		if (op.function.init_global) {
			auto filters = table_filters ? *table_filters : GetTableFilters(op);
			TableFunctionInitInput input(op.bind_data.get(), op.column_ids, op.projection_ids, filters,
			                             op.extra_info.sample_options);

			global_state = op.function.init_global(context, input);
			if (global_state) {
				max_threads = global_state->MaxThreads();
			}
		} else {
			max_threads = 1;
		}
		if (op.function.in_out_function) {
			// this is an in-out function, we need to setup the input chunk
			vector<LogicalType> input_types;
			for (auto &param : op.parameters) {
				input_types.push_back(param.type());
			}
			input_chunk.Initialize(context, input_types);
			for (idx_t c = 0; c < op.parameters.size(); c++) {
				input_chunk.data[c].Reference(op.parameters[c]);
			}
			input_chunk.SetCardinality(1);
		}
	}

	idx_t max_threads = 0;
	unique_ptr<GlobalTableFunctionState> global_state;
	bool in_out_final = false;
	DataChunk input_chunk;
	//! Combined table filters, if we have dynamic filters
	unique_ptr<TableFilterSet> table_filters;

	optional_ptr<TableFilterSet> GetTableFilters(const PhysicalTableScan &op) const {
		return table_filters ? table_filters.get() : op.table_filters.get();
	}
	idx_t MaxThreads() override {
		return max_threads;
	}
};

void BMTableScan::BMTPCDS_Q3(ExecutionContext &context, const PhysicalTableScan &op)
{
    auto &dt_table = Catalog::GetEntry<TableCatalogEntry>(context.client, "", "", "date_dim");
    auto &item_table = Catalog::GetEntry<TableCatalogEntry>(context.client, "", "", "item");
    auto &store_sales_table = Catalog::GetEntry<TableCatalogEntry>(context.client, "", "", "store_sales");

    int test_count1 = 0;
    int test_count2 = 0;

    // 1) scan date_dim to get d_date_sk -> d_year for d_moy = 11
    std::unordered_map<int32_t,int32_t> date_dim_map;
    {
        auto &dt_transaction = DuckTransaction::Get(context.client, dt_table.catalog);
        TableScanState dt_scan_state;
        TableScanGlobalSourceState gs(context.client, op);
        vector<StorageIndex> storage_column_ids;
        storage_column_ids.push_back(StorageIndex(0)); // d_date_sk
        storage_column_ids.push_back(StorageIndex(6)); // d_year
        storage_column_ids.push_back(StorageIndex(8)); // d_moy
        dt_table.GetStorage().InitializeScan(context.client, dt_transaction, dt_scan_state, storage_column_ids);
        vector<LogicalType> types;
        types.push_back(dt_table.GetColumns().GetColumnTypes()[0]);
        types.push_back(dt_table.GetColumns().GetColumnTypes()[6]);
        types.push_back(dt_table.GetColumns().GetColumnTypes()[8]);
        while(true) {
            DataChunk result;
            result.Initialize(context.client, types);
            dt_table.GetStorage().Scan(dt_transaction, result, dt_scan_state);
            if(result.size() == 0) break;

            auto &d_date_sk = result.data[0];
            auto &d_year = result.data[1];
            auto &d_moy = result.data[2];
            auto d_date_sk_data = FlatVector::GetData<int32_t>(d_date_sk);
            auto d_year_data = FlatVector::GetData<int32_t>(d_year);
            auto d_moy_data = FlatVector::GetData<int32_t>(d_moy);

            for(int i = 0; i < result.size(); i++) {
                if(d_moy_data[i] == 11 && d_date_sk_data[i] >=2450816 && d_date_sk_data[i] <= 2452642) {
                    date_dim_map[d_date_sk_data[i]] = d_year_data[i];
                    test_count1++;
                }
            }
        }
    }

    // 2) scan item to get items with i_manufact_id = 128 and map i_item_sk -> (brand_id, brand)
    struct ItemInfo { int32_t brand_id; std::string brand; };
    std::unordered_map<int32_t, ItemInfo> item_map;
    {
        auto &item_transaction = DuckTransaction::Get(context.client, item_table.catalog);
        TableScanState item_scan_state;
        TableScanGlobalSourceState gs(context.client, op);
        vector<StorageIndex> storage_column_ids;
        storage_column_ids.push_back(StorageIndex(0));  // i_item_sk
        storage_column_ids.push_back(StorageIndex(7));  // i_brand_id
        storage_column_ids.push_back(StorageIndex(8));  // i_brand
        storage_column_ids.push_back(StorageIndex(13)); // i_manufact_id
        item_table.GetStorage().InitializeScan(context.client, item_transaction, item_scan_state, storage_column_ids);
        vector<LogicalType> types;
        types.push_back(item_table.GetColumns().GetColumnTypes()[0]);
        types.push_back(item_table.GetColumns().GetColumnTypes()[7]);
        types.push_back(item_table.GetColumns().GetColumnTypes()[8]);
        types.push_back(item_table.GetColumns().GetColumnTypes()[13]);
        while(true) {
            DataChunk result;
            result.Initialize(context.client, types);
            item_table.GetStorage().Scan(item_transaction, result, item_scan_state);
            if(result.size() == 0) break;

            auto &i_item_sk = result.data[0];
            auto &i_brand_id = result.data[1];
            auto &i_brand = result.data[2];
            auto &i_manufact_id = result.data[3];
            auto i_item_sk_data = FlatVector::GetData<int64_t>(i_item_sk);
            auto i_brand_id_data = FlatVector::GetData<int32_t>(i_brand_id);
            auto i_brand_data = FlatVector::GetData<string_t>(i_brand);
            auto i_manufact_id_data = FlatVector::GetData<int32_t>(i_manufact_id);

            for(int i = 0; i < result.size(); i++) {
                if(i_manufact_id_data[i] == 128) {
                    item_map[i_item_sk_data[i]] = {i_brand_id_data[i], i_brand_data[i].GetString()};
                    test_count2++;
                }
            }
        }
    }

    // 3) use bitmap for store_sales.ss_sold_date_sk to get rowids for all date_sk in date_sk_to_year
    struct GroupKey {
        int32_t d_year;
        int32_t i_brand_id;
        std::string i_brand;

        bool operator<(const GroupKey &other) const {
            if (d_year != other.d_year) return d_year < other.d_year;
            if (i_brand_id != other.i_brand_id) return i_brand_id < other.i_brand_id;
            return i_brand < other.i_brand;
        }
    };
    std::map<GroupKey, int32_t> group_map;

    // {
    //     auto rabit_ss_item_sk = dynamic_cast<rabit::Rabit *>(context.client.bitmap_ss_item_sk);

    //     auto &store_sales_transaction = DuckTransaction::Get(context.client, store_sales_table.catalog);
	// 	TableScanState store_sales_scan_state;
	// 	TableScanGlobalSourceState gs(context.client, op);
	// 	vector<StorageIndex> storage_column_ids;
	// 	storage_column_ids.push_back(StorageIndex(0)); // ss_sold_date_sk
    //     storage_column_ids.push_back(StorageIndex(2)); // ss_item_sk
	// 	storage_column_ids.push_back(StorageIndex(15)); // ss_ext_sales_price
	// 	store_sales_table.GetStorage().InitializeScan(context.client, store_sales_transaction, store_sales_scan_state, storage_column_ids);
	// 	vector<LogicalType> types;
	// 	types.push_back(store_sales_table.GetColumns().GetColumnTypes()[0]);
    //     types.push_back(store_sales_table.GetColumns().GetColumnTypes()[2]);
	// 	types.push_back(store_sales_table.GetColumns().GetColumnTypes()[15]);

    //     vector<row_t> *ids = new vector<row_t>;
	// 	size_t cursor = 0;

    //     ibis::bitvector btv_res;

    //     auto it = item_map.begin();
    //     btv_res.copy(*rabit_ss_item_sk->Btvs[it->first]->btv);
	// 	btv_res.decompress();
	// 	it++;

    //     while(it != item_map.end()) {
	// 		btv_res |= *rabit_ss_item_sk->Btvs[it->first]->btv;
	// 		it++;
	// 	}

    //     GetRowids(btv_res, ids);

    //     while(true) {		
	// 		DataChunk result;
	// 		result.Initialize(context.client, types);

	// 		if(cursor < ids->size()) {
	// 			ColumnFetchState column_fetch_state;
	// 			data_ptr_t row_ids_data = nullptr;
	// 			row_ids_data = (data_ptr_t)&((*ids)[cursor]);
	// 			Vector row_ids_vec(LogicalType::ROW_TYPE, row_ids_data);
	// 			idx_t fetch_count = 2048;
	// 			if(cursor + fetch_count > ids->size()) {
	// 				fetch_count = ids->size() - cursor;
	// 			}
	// 			store_sales_table.GetStorage().BMFetch(store_sales_transaction, result, storage_column_ids, row_ids_vec, fetch_count,
	// 													column_fetch_state, num_idlist);

	// 			cursor += fetch_count;
	// 		}
	// 		else {
	// 			delete ids;
	// 			break;
	// 		}

	// 		auto &sold_date_sk = result.data[0];
    //         auto &ss_item_sk = result.data[1];
    //         auto &ext_sales_price = result.data[2];

	// 		auto sold_date_sk_data = FlatVector::GetData<int32_t>(sold_date_sk);
	// 		auto ss_item_sk_data = FlatVector::GetData<int32_t>(ss_item_sk);
	// 		auto ext_sales_price_data = FlatVector::GetData<int32_t>(ext_sales_price);

	// 		for(int i = 0; i < result.size(); i++) {
    //         auto date_it = date_dim_map.find(sold_date_sk_data[i]);
    //         if (date_it != date_dim_map.end()) {
    //             const auto &item = item_map.at(ss_item_sk_data[i]);
    //             GroupKey key;
    //             key.d_year = date_it->second;
    //             key.i_brand_id = item.brand_id;
    //             key.i_brand = item.brand;
    //             group_map[key] += ext_sales_price_data[i];
    //         }
    //     }
	// 	}
    // }

    {
        auto &store_sales_transaction = DuckTransaction::Get(context.client, store_sales_table.catalog);
		TableScanState store_sales_scan_state;
		TableScanGlobalSourceState gs(context.client, op);
		vector<StorageIndex> storage_column_ids;
		storage_column_ids.push_back(StorageIndex(0)); // ss_sold_date_sk
        storage_column_ids.push_back(StorageIndex(2)); // ss_item_sk
		storage_column_ids.push_back(StorageIndex(15)); // ss_ext_sales_price
		store_sales_table.GetStorage().InitializeScan(context.client, store_sales_transaction, store_sales_scan_state, storage_column_ids);
		vector<LogicalType> types;
		types.push_back(store_sales_table.GetColumns().GetColumnTypes()[0]);
        types.push_back(store_sales_table.GetColumns().GetColumnTypes()[2]);
		types.push_back(store_sales_table.GetColumns().GetColumnTypes()[15]);

        while(true) {		
			DataChunk result;
			result.Initialize(context.client, types);

			store_sales_table.GetStorage().Scan(store_sales_transaction, result, store_sales_scan_state);
            if(result.size() == 0) break;

			auto &sold_date_sk = result.data[0];
            auto &ss_item_sk = result.data[1];
            auto &ext_sales_price = result.data[2];

			auto sold_date_sk_data = FlatVector::GetData<int32_t>(sold_date_sk);
			auto ss_item_sk_data = FlatVector::GetData<int32_t>(ss_item_sk);
			auto ext_sales_price_data = FlatVector::GetData<int32_t>(ext_sales_price);

            for(int i = 0; i < result.size(); i++) {
                auto item_it = item_map.find(ss_item_sk_data[i]);
                if (item_it != item_map.end()) {
                    auto date_it = date_dim_map.find(sold_date_sk_data[i]);
                    if (date_it != date_dim_map.end()) {
                        GroupKey key;
                        key.d_year = date_it->second;
                        key.i_brand_id = item_it->second.brand_id;
                        key.i_brand = item_it->second.brand;
                        group_map[key] += ext_sales_price_data[i];
                    }
                }
            }
		}
    }

    std::vector<std::pair<GroupKey, int32_t>> group_vec(group_map.begin(), group_map.end());

    std::sort(group_vec.begin(), group_vec.end(),
        [](const std::pair<GroupKey, int32_t>& a, const std::pair<GroupKey, int32_t>& b) {
            if (a.first.d_year != b.first.d_year) return a.first.d_year < b.first.d_year;
            if (a.second != b.second) return a.second > b.second; // sum_agg DESC
            return a.first.i_brand_id < b.first.i_brand_id;
        }
    );

    int limit = 100;
    for (int i = 0; i < (int)group_vec.size() && i < limit; i++) {
        const auto& key = group_vec[i].first;
        int32_t sum_agg = group_vec[i].second;
        std::cout << key.d_year << "\t" << key.i_brand_id << "\t" << key.i_brand <<"\t"<< std::fixed << std::setprecision(2) << ((double)sum_agg) / 100 << std::endl;
    }

    return;
}
}