<?php error_reporting(E_ALL);

$script = null;
$options = [];
$newArgs = [];

for ($i = 1; $i < $argc; ++$i) {
    $arg = $argv[$i];
    if (null === $script) {
        if (0 === strpos($arg, '--')) {
            if (false !== $pos = strpos($arg, '=')) {
                $options[substr($arg, 2, $pos-2)] = substr($arg, $pos+1);
            } else {
                $options[substr($arg, 2)] = true;
            }
        } else {
            $script = $arg;
            $newArgs[] = $arg;
        }
    } else {
        $newArgs[] = $arg;
    }
}

if (isset($options['html'])) {
    $type = 'html';
    unset($options['html']);
} elseif (isset($options['callgrind'])) {
    $type = 'callgrind';
    unset($options['callgrind']);
} elseif (isset($options['DEBUG'])) {
    $type = 'DEBUG';
    unset($options['DEBUG']);
} else {
    $type = 'html';
}

if (isset($options['interval'])) {
    $sampleInterval = $options['interval'];
    unset($options['interval']);
} else {
    $sampleInterval = 0;
}

if (null === $script || !empty($options)) {
    die("Usage: php sample_prof.php [--html | --callgrind] [--interval=usecs] script.php ...args\n");
}

$argv = $newArgs;
$argc = count($argv);

ob_start();
sample_prof_start($sampleInterval);
require $script;
sample_prof_end();
$output = ob_get_clean();
$data = sample_prof_get_data();

if ('html' === $type) {
    echo <<<'HEADER'
<style>
    .file {
        border-collapse: collapse;
    }
    .file td {
        padding-right: 10px;
    }
    .lines {
        vertical-align: top;
        text-align: right;
        font-family: monospace;
        color: grey;
    }
    .nums {
        vertical-align: top;
        text-align: right;
        font-family: monospace;
    }
    .code {
        vertical-align: top;
    }
</style>
HEADER;

    echo '<h4>Output:</h4><pre>' . $output . '</pre>';

    foreach ($data as $file => $lines) {
        $linesInFile = substr_count(file_get_contents($file), "\n") + 1;
        $maxNum = max($lines);

        echo '<h4>' . $file . ':</h4>';
        echo '<table class="file"><tr><td class="lines">';
        echo implode('<br>', range(1, $linesInFile)); 
        echo '</td><td class="nums">';
        for ($i = 1; $i <= $linesInFile; ++$i) {
            if (isset($lines[$i])) {
                $perc = round($lines[$i] / $maxNum * 0.7, 2);
                echo '<div style="background-color: rgba(255, 0, 0, ' . $perc . ');">' . $lines[$i] . '</div>';
            } else {
                echo '<div><br /></div>';
            }
        }
        echo '</td><td class="code">';
        highlight_file($file);
        echo '</td></tr></table>';
        echo "\n";
    }
} elseif ('callgrind' === $type) {
    echo "events: Hits\n";

    $functions = collect_functions();
    foreach ($data as $file => $lines) {
        // Group by function
        $funcData = [];
        foreach ($lines as $line => $hits) {
            $func = find_function_at_pos($functions, $file, $line);
            if (null === $func) {
                $func = '{unknown}';
            }
            $funcData[$func][$line] = $hits;
        }

        // Print data
        foreach ($funcData as $func => $data) {
            echo "\nfl=$file\n";
            echo "fn=$func\n";
            foreach ($data as $line => $hits) {
                echo "$line $hits\n";
            }
        }
    }
} elseif ('DEBUG' === $type) {
    echo $output, "\n";

    $hitsPerFile = array_map('array_sum', $data);
    $totalHits = array_sum($hitsPerFile);

    echo 'Files: ', count($data), "\n";
    echo 'Total hits: ', $totalHits, "\n";
}

function collect_functions() {
    $functions = [];

    $classes = get_declared_classes();
    foreach ($classes as $class) {
        $rc = new ReflectionClass($class);
        $fileName = $rc->getFileName();
        foreach ($rc->getMethods() as $rm) {
            $name = $class . "::" . $rm->name;
            $functions[$fileName][] = [$name, $rm->getStartLine(), $rm->getEndLine()];
        }
    }

    $userFunctions = get_defined_functions()['user'];
    foreach ($userFunctions as $function) {
        $rf = new ReflectionFunction($function);
        $fileName = $rf->getFileName();
        $functions[$fileName][] = [$function, $rf->getStartLine(), $rf->getEndLine()];
    }

    return $functions;
}

function find_function_at_pos($functions, $fileName, $line) {
    if (!isset($functions[$fileName])) {
        return null;
    }

    foreach ($functions[$fileName] as list($name, $startLine, $endLine)) {
        if ($line >= $startLine && $line <= $endLine) {
            return $name;
        }
    }

    return null;
}
