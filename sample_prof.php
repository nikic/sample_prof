<?php error_reporting(E_ALL);

$script = null;
$options = [];
$newArgs = [];

for ($i = 1; $i < $argc; ++$i) {
    $arg = $argv[$i];
    if (0 === strpos($arg, '--')) {
        $options[substr($arg, 2)] = true;
    } elseif (null === $script) {
        $script = $arg;
        $newArgs[] = $arg;
    } else {
        $newArgs[] = $arg;
    }
}

if (empty($options) || isset($options['html'])) {
    $type = 'html';
} elseif (isset($options['callgrind'])) {
    $type = 'callgrind';
}

if (null === $script || count($options) > 1) {
    die('Usage: php sample_prof.php [--html | --callgrind] script.php ...args');
}

$argv = $newArgs;
$argc = count($argv);

ob_start();
sample_prof_start(1);
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
    }
} elseif ('callgrind' === $type) {
    echo "events: Hits\n";

    foreach ($data as $file => $lines) {
        echo "\nfi=$file\n";
        echo "fn=$file\n";
        foreach ($lines as $line => $hits) {
            echo "$line $hits\n";
        }
    }
}
