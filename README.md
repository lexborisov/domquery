# PHP Modest Wrapper #

This is PHP wrapper (or binding) for fast HTML renderer [Modest](https://github.com/lexborisov/Modest).

## Synopsys ##

```php
$html = <<<EOHTML
<html>
<header><title>Duracell Batteries</title></header>
<body>
    <a href="/spark/ref=nav_upnav_merged_T1_Detail">From The Community</a>
    <a href="/ref=nav_logo" class="nav-logo-link" tabindex="6">Amazon</a>
</body>
</html>
EOHTML;

$doc = ModestDocument($html);

$t = $doc->find("title");
echo "Title: " . $t->item(0).text() . "\n";

$refs = $doc->find("a");
echo "Found " . $refs->count() . " refs\n";

foreach($refs as $a) {
    echo "  - " . $a->attr("href") . "\n";
    echo "    " . $a->innerHTML() . "\n";
}

```

## Installation ##

### Prerequisites ###

* GNU Make
* gcc
* PHP + PHP devel (5.6 and 7.1 tested)
* autoconf

### Build ###

```bash
git clone https://github.com/lexborisov/Modest.git
make -C Modest
phpize5.6
./configure --enable-modest --with-php-config=php-config5.6 CPPFLAGS="-IModest/include" LDFLAGS="-LModest/lib"
make
php5.6 -d "extension=modules/modest.so" test.php
```

## Implemented Methods ##

ModestDocument(<html_string>) - document constructor.

Node (and Document) methods:

* find(<selector>)
* innerHTML()
* next()
* outerHTML()
* previous()
* text()

