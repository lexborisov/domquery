#ifndef PHP_MODEST_H
#define PHP_MODEST_H

#define PHP_MODEST_EXTNAME  "modest"
#define PHP_MODEST_EXTVER   "0.1"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 

#include "php.h"

extern zend_module_entry modest_module_entry;
#define phpext_modest_ptr &modest_module_entry;

#endif /* PHP_MODEST_H */
