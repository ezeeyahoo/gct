#if !defined(GLOBUS_XIO_PASS_MACRO_H)
#define GLOBUS_XIO_PASS_MACRO_H 1

#include "globus_common.h"

/************************************************************************
 *                      attribute macros
 ***********************************************************************/
#define GlobusIXIOAttrGetDS(ds, attr, driver)                               \
do                                                                          \
{                                                                           \
    int                                         _ctr;                       \
    globus_i_xio_attr_t *                       _attr;                      \
    globus_xio_driver_t                         _driver;                    \
    globus_i_xio_attr_ent_t *                   _entry;                     \
    void *                                      _ds = NULL;                 \
                                                                            \
    _attr = (attr);                                                         \
    _driver = (driver);                                                     \
                                                                            \
    _entry = _attr->entry;                                                  \
    for(_ctr = 0; _ctr < _attr->ndx && _ds == NULL; _ctr++)                 \
    {                                                                       \
        if(_entry[_ctr].driver == driver)                                   \
        {                                                                   \
            _ds = entry[ctr].driver_data;                                   \
        }                                                                   \
    }                                                                       \
    ds = _ds;                                                               \
} while(0)
    
#define GlobusIXIODDGetDS(ds, dd, driver)                                   \
do                                                                          \
{                                                                           \
    int                                         _ctr;                       \
    globus_i_xio_dd_t *                         _dd;                        \
    globus_xio_driver_t                         _driver;                    \
    globus_i_xio_attr_ent_t *                   _entry;                     \
    void *                                      _ds = NULL;                 \
                                                                            \
    _dd = (dd);                                                             \
    _driver = (driver);                                                     \
                                                                            \
    _entry = _dd->entry;                                                    \
    for(_ctr = 0; _ctr < _dd->stack_size && _ds == NULL; _ctr++)            \
    {                                                                       \
        if(_entry[_ctr].driver == driver)                                   \
        {                                                                   \
            _ds = entry[ctr].driver_data;                                   \
        }                                                                   \
    }                                                                       \
    ds = _ds;                                                               \
} while(0)

#define GlobusXIODriverPassServerAccept(res, op, cb, user_arg)              \
do                                                                          \
{                                                                           \
    globus_i_xio_op_t *                             _op;                    \
    globus_i_xio_server_t *                         _server;                \
    globus_i_xio_server_entry_t *                   _next_entry;            \
    globus_i_xio_server_entry_t *                   _my_entry;              \
                                                                            \
    _op = (globus_i_xio_op_t *)(op);                                        \
    _server = _op->server;                                                  \
                                                                            \
    globus_assert(_op->ndx < _server->stack_size);                          \
    if(_op->canceled)                                                       \
    {                                                                       \
        out_res = OperationHasBeenCacneled();                               \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        _op->progress = GLOBUS_TRUE;                                        \
        _my_entry = &_server->entry[_op->ndx];                              \
        _my_entry->cb = (cb);                                               \
        _my_entry->user_arg = (user_arg);                                   \
        _my_entry->in_register = GLOBUS_TRUE;                               \
        do                                                                  \
        {                                                                   \
            _op->ndx++;                                                     \
            _next_entry = &_server->entry[_op->ndx];                        \
        }                                                                   \
        while(_next_entry->driver->server_accept_func == NULL)              \
                                                                            \
        /* at time that stack is built this will be varified */             \
        globus_assert(_op->ndx <= _server->stack_size);                     \
        res = _next_entry->driver->server_accept_func(                      \
                    _next_entry->server_handle,                             \
                    _op->entry[_op->ndx]._op_ent_accept_attr,               \
                    _op);                                                   \
        _my_entry->in_register = GLOBUS_FALSE;                              \
    }                                                                       \
}

#define GlobusXIODriverFinishedAccept(server, target, result)               \
do                                                                          \
{                                                                           \
    globus_i_xio_op_t *                             _op;                    \
    globus_i_xio_server_entry_t *                   _entry;                 \
    globus_i_xio_server_t *                         _server;                \
                                                                            \
    _op = (globus_i_xio_op_t *)(op);                                        \
    _server = _op->server;                                                  \
    globus_assert(_server->ndx > 0);                                        \
    _op = GLOBUS_TRUE;                                                      \
                                                                            \
    _op->entry[_op->ndx].target = (target);                                 \
                                                                            \
    do                                                                      \
    {                                                                       \
        _op->ndx--;                                                         \
    }                                                                       \
    while(_server->entry[_op->ndx].cb == NULL &&                            \
            _op->ndx != 0)                                                  \
                                                                            \
    if(_server->entry[_op->ndx].in_register)                                \
    {                                                                       \
        _server->cached_res = (result);                                     \
        globus_callback_space_register_oneshot(                             \
            NULL,                                                           \
            NULL,                                                           \
            globus_l_xio_serveraccept_kickout,                              \
            (void *)_server,                                                \
            GLOBUS_CALLBACK_GLOBAL_SPACE);                                  \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        _server->entry[_op->ndx].cb(_server, result,                        \
            _server->entry[_op->ndx].user_arg);                             \
    }                                                                       \
} while(0)

#define GlobusXIOServerEnableCancel(server, canceled, cb, user_arg)         \
do                                                                          \
{                                                                           \
    globus_i_xio_server_t *                         _server;                \
                                                                            \
    _server = (globus_i_xio_server_t *)(server);                            \
    globus_mutex_lock(&_server->mutex);                                     \
    {                                                                       \
        canceled = _server->canceled;                                       \
        if(!canceled)                                                       \
        {                                                                   \
            _server->cancel_cb = (cb);                                      \
            _server->cancel_user_arg = (user_arg);                          \
        }                                                                   \
    }                                                                       \
    globus_mutex_unlock(&_server->mutex);                                   \
} while(0)

#define GlobusXIOServerDisableCancel(server)                                \
do                                                                          \
{                                                                           \
    globus_i_xio_server_t *                         _server;                \
                                                                            \
    _server = (globus_i_xio_server_t *)(server);                            \
    globus_mutex_lock(&_server->mutex);                                     \
    {                                                                       \
        _server->cancel_cb = NULL;                                          \
        _server->cancel_user_arg = NULL;                                    \
    }                                                                       \
    globus_mutex_unlock(&_server->mutex);                                   \
} while(0)



#define                                                                 \
Globus_XIO_Driver_Pass_Write(out_res, op, iovec, iovec_count, cb,       \
    user_arg)                                                           \
do                                                                      \
{                                                                       \
    globus_i_xio_operation_t *                      _op;                \
    globus_xio_data_callback_t                      _cb;                \
    void *                                          _user_arg;          \
    globus_iovec_t *                                _iovec;             \
    int                                             _iovec_count;       \
    globus_i_xio_op_entry_t *                       _entry;             \
                                                                        \
    _op = (op);                                                         \
    _iovec = (iovec);                                                   \
    _iovec_count = (iovec_count);                                       \
    _cb = (cb);                                                         \
    _user_arg = (user_arg);                                             \
                                                                        \
    if(_op->canceled)                                                   \
    {                                                                   \
        out_res = GlobusXIOErrorOperationCanceled(                      \
                        "Globus_XIO_Driver_Pass_Write");                \
    }                                                                   \
    /* if at bottom of the stack we cannot pass further down */         \
    else if(_op->ndx == _op->stack_size)                                \
    {                                                                   \
        out_res = GlobusXIOErrorPassToFar(                              \
                        "Globus_XIO_Driver_Pass_Write");                \
    }                                                                   \
    else                                                                \
    {                                                                   \
        _op->progress = GLOBUS_TRUE;                                    \
        _entry = &_op->entry_array[_op->ndx];                           \
        _entry->cb = _cb; /* this may result in a casting error */      \
        _entry->user_arg = _user_arg;                                   \
        _entry->iovec = iovec;                                          \
        _entry->iovec_count = iovec_count;                              \
        /* set the callstack flag */                                    \
        _entry->in_register = GLOBUS_TRUE;                              \
        /* find next slot. start on next and find first interseted */   \
        _op->ndx++;                                                     \
        while(_op->context[_op->ndx]->driver->write_func == NULL &&     \
              _op->ndx < _op->stack_size)                               \
        {                                                               \
            _op->ndx++;                                                 \
        }                                                               \
                                                                        \
        _context = &_op->context[_op->ndx];                             \
        out_res = context->driver->write_func(                          \
                        _context->driver_handle,                        \
                        _entry->iovec,                                  \
                        _entry->iovec_count,                            \
                        _op);                                           \
                                                                        \
        /* flip the callstack flag */                                   \
        _entry->in_register = GLOBUS_FALSE;                             \
        out_context = _op->context;                                     \
    }                                                                   \
} while(0)


#define                                                                 \
Globus_XIO_Driver_Finished_Write(op, result, nwritten)                  \
do                                                                      \
{                                                                       \
    globus_result_t                                 _res;               \
    globus_i_xio_operation_t *                      _op;                \
    globus_i_xio_op_entry_t *                       _entry;             \
                                                                        \
    /* set local pointers for all macro parameters */                   \
    _op = (op);                                                         \
    _res = (result);                                                    \
                                                                        \
    assert(_op->ndx > 0);                                               \
    _op->progress = GLOBUS_TRUE;                                        \
    /*                                                                  \
     * this means that we are finishing with a different context        \
     * copy the finishing one into the operations;                      \
     */                                                                 \
    if(_op->context != _context && _context != NULL)                    \
    {                                                                   \
        /* iterate through them all and copy handles into new slot */   \
        for(ctr = _op->ndx; ctr < _op->stack_size; ctr++)               \
        {                                                               \
            _op->context[ctr].driver_handle =                           \
                _control[ctr].driver_handle;                            \
        }                                                               \
    }                                                                   \
    /*                                                                  \
     * if limited we will do nothing here                               \
     */                                                                 \
    if(!_op->entry_array[_op->ndx]->is_limited)                         \
    {                                                                   \
        /* find next interested driver */                               \
        _op->ndx--;                                                     \
        while(_op->entry_array[_op->ndx].data_cb == NULL &&             \
                _op->ndx != 0)                                          \
        {                                                               \
            _op->ndx--;                                                 \
        }                                                               \
        _entry = &_op->entry_array[_op->ndx];                           \
        _entry->nbytes = (nwritten);                                    \
        /* if one is found call it, otherwise no callback is called */  \
        if(_entry->data_cb)                                             \
        {                                                               \
            /* if in register thread then kick out a 1 shot */          \
            if(entry->in_register = GLOBUS_FALSE)                       \
            {                                                           \
                _op->cached_res = _res;                                 \
                globus_callback_space_register_oneshot(                 \
                    NULL,                                               \
                    NULL,                                               \
                    globus_l_xio_data_driver_kickout,                   \
                    (void *)_op,                                        \
                    GLOBUS_CALLBACK_GLOBAL_SPACE);                      \
            }                                                           \
            /* if not in a register just call it */                     \
            else                                                        \
            {                                                           \
                _entry->data_cb(_op, _res, _nwritten, _entry->user_arg);\
            }                                                           \
        }                                                               \
    }                                                                   \
} while (0)


#define \
Globus_XIO_Driver_Pass_Open(out_res, out_context, op, cb, user_arg)     \
do                                                                      \
{                                                                       \
    globus_i_xio_operation_t *                      _op;                \
    globus_xio_callback_t                           _cb;                \
    void *                                          _user_arg;          \
    globus_i_xio_context_t *                        _context;           \
    globus_i_xio_op_entry_t *                       _entry;             \
                                                                        \
    _op = (op);                                                         \
    _cb = (cb);                                                         \
    _user_arg = (user_arg);                                             \
                                                                        \
    /* if an error occurs it is up to level above to clean up */        \
    /*                                                                  \
     *  if op is canceled just return an error                          \
     *  error can then be passed on back up the stack with finished op  \
     */                                                                 \
    if(_op->canceled)                                                   \
    {                                                                   \
        out_res = GlobusXIOErrorOperationCanceled(                      \
                        "Globus_XIO_Driver_Pass_Write");                \
    }                                                                   \
    /* if at bottom of the stack we cannot pass further down */         \
    else if(_op->ndx == _op->stack_size)                                \
    {                                                                   \
        out_res = GlobusXIOErrorPassToFar(                              \
                        "Globus_XIO_Driver_Pass_Write");                \
    }                                                                   \
    else                                                                \
    {                                                                   \
        /* state that progress has been made */                         \
        _op->progress = GLOBUS_TRUE;                                    \
        /* assign callback info in current slot */                      \
        _entry = &_op->entry_array[_op->ndx];                           \
        _entry->cb = _cb; /* this may result in a casting error */      \
        _entry->user_arg = _user_arg;                                   \
        /* set the callstack flag */                                    \
        _entry->in_register = GLOBUS_TRUE;                              \
        /* find next slot. start on next and find first interseted */   \
        _op->ndx++;                                                     \
        while(_op->context[_op->ndx]->driver->open_func == NULL &&      \
              _op->ndx < _op->stack_size)                               \
        {                                                               \
            _op->ndx++;                                                 \
        }                                                               \
                                                                        \
        /*                                                              \
         * if a open function is not found anywhere on the stack we have \
         * problems.  this hsould not happen ever, it is checked when   \
         * the stack is built.  if this does happen there is most likely\
         * memory curroption                                            \
         */                                                             \
        assert(_op->context[_op->ndx]->driver->open_func != NULL);      \
                                                                        \
        /* call the next interface function in the stack */             \
        _context = &_op->context[_op->ndx];                             \
        out_res = context->driver->open_func(                           \
                        &_context->driver_handle,                       \
                        _context->driver_attr,                          \
                        _context->target,                               \
                        _op->context,                                   \
                        _op);                                           \
                                                                        \
        /* flip the callstack flag */                                   \
        _entry->in_register = GLOBUS_FALSE;                             \
        out_context = _op->context;                                     \
    }                                                                   \
} while(0)

Globus_XIO_Driver_Finished_Open(context, open_op, res)
do 
{
    globus_result_t                                 _res;
    globus_i_xio_context_t *                        _context;
    globus_i_xio_operation_t *                      _op;
    globus_i_xio_op_entry_t *                       _entry;

    /* set local pointers for all macro parameters */
    _op = (open_op);
    _context = (context);
    _res = (res);

    assert(_op->ndx > 0);
    _op->progress = GLOBUS_TRUE;
    /* 
     * this means that we are finishing with a different context 
     * copy the finishing one into the operations;
     */
    if(_op->context != _context && _context != NULL)
    {
        /* iterate through them all and copy handles into new slot */
        for(ctr = _op->ndx; ctr < _op->stack_size; ctr++)
        {
            _op->context[ctr].driver_handle = _control[ctr].driver_handle;
        }
    }
    /*
     * if limited we will do nothing here 
     */
    if(!_op->entry_array[_op->ndx]->is_limited)
    {
        /* find next interested driver */
        _op->ndx--; 
        while(_op->entry_array[_op->ndx].cb == NULL && _op->ndx != 0)
        {
            _op->ndx--; 
        }
        _entry = &_op->entry_array[_op->ndx];
        /* if one is found call it, otherwise no callback is called */
        if(_entry->cb)
        {
            /* if in register thread then kick out a 1 shot */
            if(entry->in_register = GLOBUS_FALSE)
            {
                GlobusTimeReltimeSet(delay_time, 0, 0);
                _op->cached_res = _res;
                globus_callback_space_register_oneshot(
                    NULL,
                    delay_time,
                    globus_l_xio_open_driver_kickout,
                    (void *)_op,
                    GLOBUS_CALLBACK_GLOBAL_SPACE);
            }
            /* if not in a register just call it */
            else
            {
                _entry->cb(_op, _res, _entry->user_arg);
            }
        }
    }
} while (0)

/*********************************************************************
 *              function signatures used by the macros
 ********************************************************************/
void
globus_l_xio_data_driver_kickout(
    void *                                      user_arg);

/* implemented in globus_xio_server.c */
void
globus_l_xio_server_driver_op_kickout(
    void *                                      user_arg);

#endif
