### About this branch

This branch includes a DuckDB with bitmap indexing (along with bitmap instances for TPC-H queries), enabling students to benchmark bitmap indexing and zone maps in DuckDB.

### Download bitmap instances

We have prepared the required bitmap instances. Please download them from https://1drv.ms/u/c/4f1a15e54aa03b95/IQBZNNczpYI_T5yUscwfIpfkAcK0HBjVEG9kT0w9_ta5f58?e=v18wwU

Then, copy the compressed file bitmap_data.tar.gz into this directory and extract it.

### How to run BitmapIndexing？

(1) Compile the project CUBIT-dev under /extension/debit/CUBIT-dev.

```sh
cd extension/debit/CUBIT-dev
./build.sh
```

(2) Compile the main project.

```sh
make
```

(3) Generate a TPCH dataset with Scale Factor SF = 10.

```DuckDB
call dbgen(sf=10);
```

(4) Run TPC-H Q6 using bitmap indexing.

```DuckDB
set threads to 1;
pragma load_bitmap(shipdate_GE,discount,quantity);
pragma bm_tpch(6);
```

(5) Run TPC-H Q6 with zone maps (the default mechanism in DuckDB)

```DuckDB
pragma tpch(6);
```

Note that we have included bitmap instances for TPC-H Q1, Q5, Q6, and Q14 in this repository (listed below). You can also see which bitmaps each query loads in extension/debit/debit_extension.cpp. If you need bitmap instances for additional queries, please contact the TAs.

- (shipdate, linestatus, returnflag) for Q1
- (orderkey, suppkey) for Q5
- (shipdate_GE_364, discount, quantity) for Q6
- (shipdate_GE_30) for Q14
