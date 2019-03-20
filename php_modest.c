#include <modest/finder/finder.h>
#include <myhtml/myhtml.h>
#include <mycss/mycss.h>
#include <myhtml/serialization.h>

#include "php_modest.h"

static zend_class_entry *node_ce;
static zend_class_entry *document_ce;
static zend_class_entry *nodelist_ce;

static HashTable classes;
static HashTable node_prop_handlers;
static HashTable document_prop_handlers;
static HashTable nodelist_prop_handlers;

static zend_object_handlers node_object_handlers;
static zend_object_handlers document_object_handlers;
static zend_object_handlers nodelist_object_handlers;


typedef struct document_object_s document_object;

static inline document_object *get_document_object(zval *object);

#if PHP_MAJOR_VERSION < 7
typedef document_object* doc_ref_t;
typedef zval* cur_obj_t;
typedef zend_object_value create_ret_t;
typedef int str_size_t;
typedef long param_long_t;

#define RETVAL_MYHTML_STR(str) RETVAL_STRINGL(str.data, str.length, 1)

#ifndef Z_OBJ_P
#define Z_OBJ_P(o) zend_object_store_get_object(o)
#endif

#else
typedef zval doc_ref_t;
typedef zval cur_obj_t;
typedef zend_object* create_ret_t;
typedef size_t str_size_t;
typedef zend_long param_long_t;

#define RETVAL_MYHTML_STR(str) RETVAL_STRINGL(str.data, str.length)

#endif


struct document_object_s {
    zend_object std;

    doc_ref_t document;
    myhtml_tree_node_t *node;


    mycss_entry_t *css;
    modest_finder_t *finder;

    myhtml_t* myhtml;
    myhtml_tree_t* tree;

#if PHP_MAJOR_VERSION < 7
    zend_object_handle handle;
#endif
};

static inline
void doc_ref_copy(doc_ref_t* dst, doc_ref_t* src)
{
#if PHP_MAJOR_VERSION < 7
    *dst = *src;
    zend_objects_store_add_ref_by_handle((*dst)->handle);
#else
    ZVAL_COPY(dst, src);
#endif
}

static inline
void doc_ref_delete(doc_ref_t* doc)
{
#if PHP_MAJOR_VERSION < 7
    zend_objects_store_del_ref_by_handle_ex((*doc)->handle, &document_object_handlers);
#else
    zval_ptr_dtor(doc);
    // --GC_REFCOUNT(doc->std)
    // zend_objects_store_del(obj);
#endif
}

static inline
document_object* get_doc_by_ref(doc_ref_t* doc)
{
#if PHP_MAJOR_VERSION < 7
    return *doc;
#else
    return get_document_object(doc);
#endif
}

typedef struct node_object_s node_object;

struct node_object_s {
    zend_object std;

    doc_ref_t document;
    myhtml_tree_node_t *node;
};

typedef struct nodelist_object_s nodelist_object;

struct nodelist_object_s {
    zend_object std;

    doc_ref_t document;

    mycss_selectors_list_t *selectors_list;
    myhtml_collection_t *collection;
};

typedef struct nodelist_iterator_s nodelist_iterator;

struct nodelist_iterator_s {
    zend_object_iterator intern;
    cur_obj_t curobj;
};


static
inline document_object *get_document_object(zval *object) {
    return (document_object *) Z_OBJ_P(object);
}

static
inline node_object *get_node_object(zval *object) {
    return (node_object *) Z_OBJ_P(object);
}

static
inline nodelist_object *get_nodelist_object(zval *object) {
    return (nodelist_object *) Z_OBJ_P(object);
}


static
inline int chk_node_valid(myhtml_tree_node_t *n) {
    // this is not 100% accurate, but should work
    return n != NULL && n->parent != NULL;
}


static
void document_dtor(zend_object *object TSRMLS_DC)
{
    document_object *doc = (document_object *) object;

    // printf("document %p dtor ... ", doc);

    modest_finder_destroy(doc->finder, true);

    mycss_t *mycss = doc->css->mycss;
    mycss_entry_destroy(doc->css, true);
    mycss_destroy(mycss, true);

    myhtml_tree_destroy(doc->tree);
    myhtml_destroy(doc->myhtml);

#if PHP_MAJOR_VERSION < 7
    doc->document = NULL;
#else
    ZVAL_UNDEF(&doc->document);
#endif

    // printf("document %p dtor done\n", doc);
}

static
void document_free(zend_object *object TSRMLS_DC)
{
    document_object *doc = (document_object *) object;

    // printf("document %p free ... ", doc);
    zend_object_std_dtor(&doc->std);

    // printf("document %p free done\n", doc);
}

#if PHP_MAJOR_VERSION < 7
static
void document_free_storage(void *object TSRMLS_DC)
{
    document_dtor(object);
    document_free(object);
}
#endif

static
create_ret_t document_create_handler(zend_class_entry *type TSRMLS_DC)
{
    document_object *doc = (document_object *) emalloc(sizeof(document_object));
    memset(doc, 0, sizeof(document_object));

    zend_object_std_init(&doc->std, type);
    object_properties_init(&doc->std, type);

    // printf("document %p create\n", doc);

#if PHP_MAJOR_VERSION < 7
    doc->document = doc;

    zend_object_value retval;
    retval.handle = zend_objects_store_put(doc, NULL, document_free_storage, NULL TSRMLS_CC);
    doc->handle = retval.handle;
    retval.handlers = &document_object_handlers;

    return retval;
#else
    ZVAL_OBJ(&doc->document, &doc->std);

    doc->std.handlers = &document_object_handlers;

    return &doc->std;
#endif
}

static mycss_entry_t *
create_css_parser(void)
{
    // base init
    mycss_t *mycss = mycss_create();
    mystatus_t status = mycss_init(mycss);

    if (status) {
        fprintf(stderr, "Can't init CSS Parser\n");
        exit(EXIT_FAILURE);
    }

    // current entry work init
    mycss_entry_t *entry = mycss_entry_create();
    status = mycss_entry_init(mycss, entry);

    if (status) {
        fprintf(stderr, "Can't init CSS Entry\n");
        exit(EXIT_FAILURE);
    }

    return entry;
}

PHP_METHOD(Document, __construct)
{
    document_object *doc = get_document_object(getThis());

    // printf("document %p construct\n", doc);

    char *value = NULL;
    str_size_t value_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &value, &value_len) == FAILURE) {
        return;
    }

    doc->css = create_css_parser();
    doc->finder = modest_finder_create_simple();

    doc->myhtml = myhtml_create();
    myhtml_init(doc->myhtml, MyHTML_OPTIONS_PARSE_MODE_SINGLE, 1, 0);

    doc->tree = myhtml_tree_create();
    myhtml_tree_init(doc->tree, doc->myhtml);

    // printf("%p parse %d bytes document ... \n", doc, (int) value_len);

    myhtml_parse(doc->tree, MyENCODING_UTF_8, value, value_len);

    doc->node = doc->tree->node_html;

    // printf("done\n");
}


static
void node_dtor(zend_object *object TSRMLS_DC)
{
    node_object *node = (node_object *) object;

    doc_ref_delete(&node->document);
}

static
void node_free(zend_object *object TSRMLS_DC)
{
    node_object *node = (node_object *) object;

    zend_object_std_dtor(&node->std);
}

#if PHP_MAJOR_VERSION < 7
static
void node_free_storage(void *object TSRMLS_DC)
{
    node_dtor(object);
    node_free(object);
}
#endif

static
create_ret_t node_create_handler(zend_class_entry *type TSRMLS_DC)
{
    node_object *node = (node_object *) emalloc(sizeof(node_object));
    memset(node, 0, sizeof(node_object));

    zend_object_std_init(&node->std, type);
    object_properties_init(&node->std, type);

#if PHP_MAJOR_VERSION < 7
    zend_object_value retval;
    retval.handle = zend_objects_store_put(node, NULL, node_free_storage, NULL TSRMLS_CC);
    retval.handlers = &node_object_handlers;

    return retval;
#else
    node->std.handlers = &node_object_handlers;

    return &node->std;
#endif
}

static
mycss_selectors_list_t *
prepare_selector(mycss_entry_t *css_entry, const char* selector, size_t selector_size)
{
    mystatus_t out_status;
    mycss_selectors_list_t *list = mycss_selectors_parse(mycss_entry_selectors(css_entry),
                                                         MyENCODING_UTF_8, selector, selector_size,
                                                         &out_status);
    /* check parsing errors */
    if(list == NULL || (list->flags & MyCSS_SELECTORS_FLAGS_SELECTOR_BAD)) {
        fprintf(stderr, "Bad CSS Selectors\n");
        exit(EXIT_FAILURE);
    }

    return list;
}

PHP_METHOD(Node, find) {
    node_object *node = get_node_object(getThis());

    char *selector = NULL;
    str_size_t selector_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &selector, &selector_len) == FAILURE) {
        return;
    }

    document_object *doc = get_doc_by_ref(&node->document);

    object_init_ex(return_value, nodelist_ce);

    nodelist_object *nl = get_nodelist_object(return_value);

    doc_ref_copy(&nl->document, &node->document);

    nl->selectors_list = prepare_selector(doc->css, selector, selector_len);
    modest_finder_by_selectors_list(doc->finder, node->node, nl->selectors_list, &nl->collection);

    // printf("collection->length = %d\n", (int) nl->collection->length);
}

mystatus_t myhtml_serialization_reallocate(mycore_string_raw_t *str, size_t size);

static
mystatus_t myhtml_serialization_concatenate(const char* data, size_t length, void *ptr)
{
    mycore_string_raw_t *str = (mycore_string_raw_t*)ptr;

    // do we still have enough size in the output buffer?
    if ((length + str->length) >= str->size) {
        if(myhtml_serialization_reallocate(str, length + str->length + 4096))
            return MyCORE_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    // copy data
    strncpy(&str->data[ str->length ], data, length);

    // update counters
    str->length += length;
    str->data[ str->length ] = '\0';

    return MyCORE_STATUS_OK;
}

PHP_METHOD(Node, text)
{
    node_object *node = get_node_object(getThis());
    mycore_string_raw_t str;

    mycore_string_raw_clean_all(&str);

    myhtml_tree_node_t *n = node->node;

    // allocate space that is most likely enough for the output
    str.size   = 4098 * 5;
    str.length = 0;
    str.data   = (char*) mycore_malloc(str.size * sizeof(char));

    if(str.data == NULL) {
        RETURN_NULL();
    }

    while (n) {
        if (n->tag_id == MyHTML_TAG__TEXT) {
            size_t length;
            const char *text = myhtml_node_text(n, &length);

            if (text != NULL) {
                myhtml_serialization_concatenate(text, length, &str);
            }
        }

        if (n->child) {
            n = n->child;
            continue;
        }

        while (n != node->node && n->next == NULL) {
            n = n->parent;
        }

        if (n == node->node) {
            break;
        }

        n = n->next;
    }

    RETVAL_MYHTML_STR(str);

    mycore_string_raw_destroy(&str, false);
}

PHP_METHOD(Node, innerHTML)
{
    node_object *node = get_node_object(getThis());
    mycore_string_raw_t str;

    mycore_string_raw_clean_all(&str);

    myhtml_tree_node_t *n = node->node->child;

    while (n) {
        myhtml_serialization_tree_buffer(n, &str);
        n = n->next;
    }

    RETVAL_MYHTML_STR(str);

    mycore_string_raw_destroy(&str, false);
}

PHP_METHOD(Node, outerHTML)
{
    node_object *node = get_node_object(getThis());
    mycore_string_raw_t str;

    mycore_string_raw_clean_all(&str);

    myhtml_serialization_tree_buffer(node->node, &str);

    RETVAL_MYHTML_STR(str);

    mycore_string_raw_destroy(&str, false);
}

PHP_METHOD(Node, attr)
{
    node_object *node = get_node_object(getThis());

    char *key = NULL;
    str_size_t key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &key, &key_len) == FAILURE) {
        return;
    }

    myhtml_tree_attr_t *attr = myhtml_attribute_by_key(node->node, key, key_len);

    if (attr == NULL) {
        RETURN_NULL();
    }

    RETVAL_MYHTML_STR(attr->value);
}

PHP_METHOD(Node, next)
{
    node_object *node = get_node_object(getThis());

    if (node->node->next == NULL) {
        RETURN_NULL();
    }

    object_init_ex(return_value, node_ce);
    node_object *next = get_node_object(return_value);

    doc_ref_copy(&next->document, &node->document);

    next->node = node->node->next;
}

PHP_METHOD(Node, previous)
{
    node_object *node = get_node_object(getThis());

    if (node->node->prev == NULL) {
        RETURN_NULL();
    }

    object_init_ex(return_value, node_ce);
    node_object *prev = get_node_object(return_value);

    doc_ref_copy(&prev->document, &node->document);

    prev->node = node->node->prev;
}

PHP_METHOD(Node, remove)
{
    node_object *node = get_node_object(getThis());

    if (chk_node_valid(node->node)) {
        myhtml_node_delete_recursive(node->node);
    }
}


static
void nodelist_dtor(zend_object *object TSRMLS_DC)
{
    nodelist_object *nodelist = (nodelist_object *) object;

    mycss_entry_t *css = get_doc_by_ref(&nodelist->document)->css;

    myhtml_collection_destroy(nodelist->collection);

    mycss_selectors_t *selectors = mycss_entry_selectors(css);

    if (selectors != NULL) {
        mycss_selectors_list_destroy(selectors,
            nodelist->selectors_list, true);
    }

    doc_ref_delete(&nodelist->document);
}

static
void nodelist_free(zend_object *object TSRMLS_DC)
{
    nodelist_object *nodelist = (nodelist_object *) object;

    zend_object_std_dtor(&nodelist->std);
}

#if PHP_MAJOR_VERSION < 7
static
void nodelist_free_storage(void *object TSRMLS_DC)
{
    nodelist_dtor(object);
    nodelist_free(object);
}
#endif

static
create_ret_t nodelist_create_handler(zend_class_entry *type TSRMLS_DC)
{
    nodelist_object *nodelist = (nodelist_object *)emalloc(sizeof(nodelist_object));
    memset(nodelist, 0, sizeof(nodelist_object));

    zend_object_std_init(&nodelist->std, type);
    object_properties_init(&nodelist->std, type);

#if PHP_MAJOR_VERSION < 7
    zend_object_value retval;
    retval.handle = zend_objects_store_put(nodelist, NULL, nodelist_free_storage, NULL TSRMLS_CC);
    retval.handlers = &node_object_handlers;
    return retval;
#else
    nodelist->std.handlers = &nodelist_object_handlers;
    return &nodelist->std;
#endif
}

PHP_METHOD(NodeList, count)
{
    nodelist_object *nodelist = get_nodelist_object(getThis());

    RETURN_LONG(nodelist->collection->length);
}

PHP_METHOD(NodeList, item)
{
    nodelist_object *nodelist = get_nodelist_object(getThis());
    param_long_t index;
    node_object *n;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &index) == FAILURE) {
        return;
    }

    if (index >= nodelist->collection->length) {
        return;
    }

    object_init_ex(return_value, node_ce);
    n = get_node_object(return_value);

    doc_ref_copy(&n->document, &nodelist->document);

    n->node = nodelist->collection->list[index];
}

PHP_METHOD(NodeList, remove)
{
    nodelist_object *nodelist = get_nodelist_object(getThis());
    myhtml_tree_node_t *n;
    size_t i;

    for (i = 0; i < nodelist->collection->length; i++) {
        n = nodelist->collection->list[i];

        if (chk_node_valid(n)) {
            myhtml_node_delete_recursive(n);
        }
    }
}


static
void nodelist_iterator_dtor(zend_object_iterator *iter)
{
    nodelist_iterator *iterator = (nodelist_iterator *)iter;

    // printf("nodelist_iterator_dtor %p\n", iter);

#if PHP_MAJOR_VERSION < 7
    zval_ptr_dtor((zval**)&iterator->intern.data);

    if (iterator->curobj) {
        zval_ptr_dtor(&iterator->curobj);
    }
#else
    zval_ptr_dtor(&iterator->intern.data);
    zval_ptr_dtor(&iterator->curobj);
#endif
}

static
int nodelist_iterator_valid(zend_object_iterator *iter)
{
    nodelist_iterator *iterator = (nodelist_iterator *)iter;

#if PHP_MAJOR_VERSION < 7
    if (iterator->curobj) {
#else
    if (Z_TYPE(iterator->curobj) != IS_UNDEF) {
#endif
        return SUCCESS;
    } else {
        return FAILURE;
    }
}

#if PHP_MAJOR_VERSION < 7
static 
void nodelist_iterator_current_data(zend_object_iterator *iter, zval ***data TSRMLS_DC)
{
    nodelist_iterator *iterator = (nodelist_iterator *)iter;

    *data = &iterator->curobj;
}
#else
static
zval *nodelist_iterator_current_data(zend_object_iterator *iter)
{
    nodelist_iterator *iterator = (nodelist_iterator *)iter;

    return &iterator->curobj;
}
#endif

static
void nodelist_iterator_current_key(zend_object_iterator *iter, zval *key)
{
    nodelist_iterator *iterator = (nodelist_iterator *)iter;

    ZVAL_LONG(key, iter->index);
}

static
void nodelist_iterator_set_curobj(nodelist_iterator *iterator, nodelist_object *nl)
{
    node_object *n;

#if PHP_MAJOR_VERSION < 7
    iterator->curobj = NULL;
#else
    ZVAL_UNDEF(&iterator->curobj);
#endif

    if (iterator->intern.index < nl->collection->length) {
#if PHP_MAJOR_VERSION < 7
        MAKE_STD_ZVAL(iterator->curobj);
        object_init_ex(iterator->curobj, node_ce);
        n = (node_object *) zend_objects_get_address(iterator->curobj TSRMLS_CC);
#else
        object_init_ex(&iterator->curobj, node_ce);
        n = get_node_object(&iterator->curobj);
#endif

        doc_ref_copy(&n->document, &nl->document);

        n->node = nl->collection->list[iterator->intern.index];
    }
}

static
void nodelist_iterator_move_forward(zend_object_iterator *iter)
{
    zval *object;
    nodelist_object *nl;
    node_object *n;

    nodelist_iterator *iterator = (nodelist_iterator *)iter;

    // printf("move forward %p, index %d\n", iter, (int) iter->index);

#if PHP_MAJOR_VERSION < 7
    object = (zval *)iterator->intern.data;
#else
    object = &iterator->intern.data;
#endif
    nl = get_nodelist_object(object);

    zval_ptr_dtor(&iterator->curobj);

    nodelist_iterator_set_curobj(iterator, nl);
}

static
zend_object_iterator_funcs nodelist_iterator_funcs = {
    nodelist_iterator_dtor,
    nodelist_iterator_valid,
    nodelist_iterator_current_data,
    nodelist_iterator_current_key,
    nodelist_iterator_move_forward,
    NULL,
    NULL
};

static
zend_object_iterator *nodelist_get_iterator(zend_class_entry *ce, zval *object, int by_ref)
{
    nodelist_object *nl = get_nodelist_object(object);
    nodelist_iterator *iterator;
    node_object *n;

    if (by_ref) {
        zend_error(E_ERROR, "An iterator cannot be used with foreach by reference");
    }

    iterator = emalloc(sizeof(nodelist_iterator));
#if PHP_MAJOR_VERSION < 7
    Z_ADDREF_P(object);
    iterator->intern.data = (void *) object;
#else
    zend_iterator_init(&iterator->intern);
    ZVAL_COPY(&iterator->intern.data, object);
#endif
    iterator->intern.funcs = &nodelist_iterator_funcs;

    iterator->intern.index = 0;

    // printf("getiterator %p, index %d of %d\n", iterator, (int) iterator->intern.index, (int) nl->collection->length);

    nodelist_iterator_set_curobj(iterator, nl);

    return &iterator->intern;
}




ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, 0, 0)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_find, 0, 0, 1)
  ZEND_ARG_INFO(0, selector)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_attr, 0, 0, 1)
  ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();

static
const zend_function_entry node_methods[] = {
    PHP_ME(Node,  find,       arginfo_find, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  text,       arginfo_none, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  innerHTML,  arginfo_none, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  outerHTML,  arginfo_none, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  attr,       arginfo_attr, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  next,       arginfo_none, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  previous,   arginfo_none, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  remove,     arginfo_none, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

ZEND_BEGIN_ARG_INFO_EX(arginfo_html, 0, 0, 1)
  ZEND_ARG_INFO(0, html)
ZEND_END_ARG_INFO();

static
const zend_function_entry document_methods[] = {
    PHP_ME(Document,  __construct,  arginfo_html, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Node,  find,         arginfo_find, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  text,         arginfo_none, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  innerHTML,    arginfo_none, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  outerHTML,    arginfo_none, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  attr,         arginfo_attr, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  next,         arginfo_none, ZEND_ACC_PUBLIC)
    PHP_ME(Node,  previous,     arginfo_none, ZEND_ACC_PUBLIC)
    PHP_FE_END
};


ZEND_BEGIN_ARG_INFO_EX(arginfo_nodelist_item, 0, 0, 1)
  ZEND_ARG_INFO(0, index)
ZEND_END_ARG_INFO();

static
const zend_function_entry nodelist_methods[] = {
    PHP_ME(NodeList,  count,  arginfo_none,           ZEND_ACC_PUBLIC)
    PHP_ME(NodeList,  item,   arginfo_nodelist_item,  ZEND_ACC_PUBLIC)
    PHP_ME(NodeList,  remove, arginfo_none,           ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(modest)
{
    zend_class_entry ce;

    zend_hash_init(&classes, 0, NULL, NULL, 1);


    INIT_CLASS_ENTRY(ce, "ModestNode", node_methods);
    node_ce = zend_register_internal_class(&ce TSRMLS_CC);
    node_ce->create_object = node_create_handler;

    memcpy(&node_object_handlers,
        zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    node_object_handlers.clone_obj = NULL;
#if PHP_MAJOR_VERSION >= 7
    node_object_handlers.dtor_obj = node_dtor;
    node_object_handlers.free_obj = node_free;
#endif

    INIT_CLASS_ENTRY(ce, "ModestDocument", document_methods);
#if PHP_MAJOR_VERSION < 7
    document_ce = zend_register_internal_class_ex(&ce, node_ce, NULL);
#else
    document_ce = zend_register_internal_class_ex(&ce, node_ce);
#endif
    document_ce->create_object = document_create_handler;
    memcpy(&document_object_handlers,
        zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    document_object_handlers.clone_obj = NULL;
#if PHP_MAJOR_VERSION >= 7
    document_object_handlers.dtor_obj = document_dtor;
    document_object_handlers.free_obj = document_free;
#endif

    INIT_CLASS_ENTRY(ce, "ModestNodeList", nodelist_methods);
    nodelist_ce = zend_register_internal_class(&ce TSRMLS_CC);
    nodelist_ce->create_object = nodelist_create_handler;
    nodelist_ce->get_iterator = nodelist_get_iterator;

    memcpy(&nodelist_object_handlers,
        zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    nodelist_object_handlers.clone_obj = NULL;
#if PHP_MAJOR_VERSION >= 7
    nodelist_object_handlers.dtor_obj = nodelist_dtor;
    nodelist_object_handlers.free_obj = nodelist_free;
#endif

    return SUCCESS;
}

zend_module_entry modest_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_MODEST_EXTNAME,
    NULL,        /* Functions */
    PHP_MINIT(modest),        /* MINIT */
    NULL,        /* MSHUTDOWN */
    NULL,        /* RINIT */
    NULL,        /* RSHUTDOWN */
    NULL,        /* MINFO */
#if ZEND_MODULE_API_NO >= 20010901
    PHP_MODEST_EXTVER,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MODEST
ZEND_GET_MODULE(modest)
#endif
