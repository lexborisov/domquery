#!/usr/bin/env php
<?php

$dir = '../temp';
$files = scandir($dir);

foreach($files as $file) {

    if ($file == '.' || $file == '..') {
        continue;
    }

    echo "file: " . $dir.'/'.$file . "\n";

    $body = file_get_contents($dir . '/' . $file);
    $document = new ModestDocument($body);

    $t = $document->find("title");
    echo "title->count() = " . $t->count() . "\n";
    $t = 0;

    $refs = $document->find("a");
    $refs_count = $refs->count();
    echo "refs->count() = " . $refs->count() . "\n";

    $refs->item(0)->remove();

    $refs = $document->find("a");
    echo "refs->count() = " . $refs->count() . "\n";

    $refs->remove();

    $refs = $document->find("a");
    echo "refs->count() = " . $refs->count() . "\n";

    $n = 0;
    foreach($refs as $n) {
        echo "id='" . $n->attr("id") . "' ";
        echo "href='" . $n->attr("href") . "' ";
        echo "class='" . $n->attr("class") . "' ";
        echo "style='" . $n->attr("style") . "'\n";

        $next = $n->next();
        if ($next) {
            echo "  next id: " . $next->attr("id") . "\n";
            echo "  next class: " . $next->attr("class") . "\n";
            $next = 0;
        }

        $prev = $n->previous();
        if ($prev) {
            echo "  prev id: " . $prev->attr("id") . "\n";
            echo "  prev class: " . $prev->attr("class") . "\n";
            $prev = 0;
        }

        # echo "  outerHTML: " . $n->outerHTML() . "\n";
        # echo "  innerHTML: " . $n->innerHTML() . "\n";
        echo "  text: " . $n->text() . "\n";
    }

    # reset variable to release node (and document) reference
    $n = 0;

    # reset variable to release nodelist (and document) reference
    $d = 0;

    # reset variable to release document
    $document = 0;
}
