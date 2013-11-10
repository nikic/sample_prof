Sampling profiler for PHP
=========================

This project implements a basic sampling profiler for PHP.

Most (all?) other profilers for PHP work by hooking into function execution (`zend_execute_ex`
to be more precise). This means that

 * they only provide resolution on the function call level, but don't specify which lines in a
   function took the longest to run.
 * they slow down code execution significantly, which often also impacts which parts are slowest.
   Thus the profiler may tell you something is slow, but it's not actually slow with profiling
   disabled.

A sampling profiler on the other hand provides line-level resolution and doesn't affect performance
to any significant degree.

Installation
------------

In the directory of the extension run:

    phpize
    ./configure
    make
    sudo make install

Usage
-----

### API

The profiler is started with `sample_prof_start($usec)`, where `$usec` is the numeric of
microseconds between sampling. The profiler is disabled with `sample_prof_end()` and the result
data can be retrieved using `sample_prof_get_data()`. The data has is an array with format
`[$file => [$line => $hits]]`. Example:

```php
<?php

sample_prof_start(1);           // start profiler with 1 usec interval
require $script;                // run script here
sample_prof_end();              // disable profiler
$data = sample_prof_get_data(); // retrieve profiling data

foreach ($data as $file => $lines) {
    echo "In file $file:\n";
    foreach ($lines as $line => $hits) {
        echo "Line $line hit $hits times.\n";
    }
}
```

### Script

A `sample_prof.php` script is provided for convenience. It is invoked as follows:

    php sample_prof.php [--html | --callgrind] script.php [...args] > out

`sample_prof.php` will run the script `script.php` with the passed `...args` and profile the
execution. The output format is specified using `--html` (default) or `--callgrind`. The former
will output HTML which can be viewed in a browser of your choice, while the latter produces a
callgrind output which you can view in a tool like KCacheGrind.

It is recommended to let `script.php` run multiple seconds to get good statistical coverage. The
longer the script runs the better the results will be.

[Sample profiling output][sample_output]: The numbers in the second column specify how often a
line has been hit. Lines that were hit more often are displayed in red.

Note that the hits are *not commulative*, i.e. if you perform a function the hits within the call
will *not* be added to the hits on the function call line.

Todo
----

 * Check how `ITIMER_PROF` interfers with the execution time limit.
 * Maybe support profiling in multi-threaded environments.
 * Check whether we can get commulative hits without impacting performance too much.
 * Check whether we can avoid getting incorrect results when the `SIGPROF` interrupt handler is
   invoked at a time where `active_op_array` has already been changed but `opline_ptr` is not
   updated yet.

  [sample_output]: http://i.imgur.com/FAID0VC.png
