# WL kernel
Thanks to Emaad A. Manzoor's hard work, our code is rewritten based on his work：

<img src="http://www3.cs.stonybrook.edu/~emanzoor/streamspot/img/streamspot-logo.jpg" height="150" align="right"/>

[http://www3.cs.stonybrook.edu/~emanzoor/streamspot/](http://www3.cs.stonybrook.edu/~emanzoor/streamspot/)

This repository contains the core streaming heterogenous graph clustering
and anomaly detection code.

Before attempting execution, ensure you have the following edge file available.

   * all-1.tsv: A file containing one edge per line for all input graphs in the dataset. The edge file used for our experiments is available at [test-data][1]. Our dataset is also from streamspot [sbustreamspot-data][2], but to accommodate our algorithm, we changed it a bit without changing the data composition.


Compilation and execution has been tested with GCC 5.2.0 .

## Quickstart

```
git clone https://github.com/WLkernel/WLkernel.git
cd WLkernel
make clean optimized

use dataset in StreamSpot for evaluation:
cat all-1.tsv|./WLkernel --edges=all-1.tsv --chunk-length=10
```

[1]: https://github.com/WLkernel/testdata.git
[2]: https://github.com/sbustreamspot/sbustreamspot-data
