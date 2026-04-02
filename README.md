
### How to run BitmapIndexing？

First, you need to compile the project CUBIT-dev under /extension/debit/CUBIT-dev.

```sh
cd extension/debit/CUBIT-dev
./build.sh
```
Then, you can compile the main project.

```sh
make
```

Second，after the compilation is finished, you can generate a TPCH dataset with a size of 10sf：

```DuckDB
call dbgen(sf=10);
```

You can view the bitmaps that each query needs to load in the file extension/debit/debit_extension.cpp.

DEBIT currently supports TPCH Q1, Q5, Q6, and Q14.
For example, if you want to run Q6, you can use the following command in DuckDB:

```DuckDB
set threads to 1;
pragma load_bitmap(shipdate_GE,discount,quantity);
pragma bm_tpch(6);
```
If you want to run the standard DuckDB TPCH queries, use the following command:

```DuckDB
pragma tpch(6);
```

Below are the required bitmap columns for each supported TPCH query in DEBIT:
  
- (shipdate, linestatus, returnflag) for Q1
- (orderkey, suppkey) for Q5
- (shipdate_GE_364, discount, quantity) for Q6
- (shipdate_GE_30) for Q14

For TPCDS Query03, you can use  the following command:
```DuckDB
pragma load_bitmap(ss_item_sk);
pragma bm_tpcds(3);
```
