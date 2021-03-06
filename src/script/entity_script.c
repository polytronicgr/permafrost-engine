/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2018 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#include <Python.h> /* must be first */
#include "entity_script.h" 
#include "../entity.h"
#include "../event.h"
#include "../asset_load.h"
#include "../anim/public/anim.h"
#include "../game/public/game.h"
#include "../lib/public/khash.h"

#include <assert.h>

typedef struct {
    PyObject_HEAD
    struct entity *ent;
}PyEntityObject;

typedef struct {
    PyEntityObject super; 
}PyAnimEntityObject;

static PyObject *PyEntity_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static void      PyEntity_dealloc(PyEntityObject *self);
static PyObject *PyEntity_get_name(PyEntityObject *self, void *closure);
static int       PyEntity_set_name(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_pos(PyEntityObject *self, void *closure);
static int       PyEntity_set_pos(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_scale(PyEntityObject *self, void *closure);
static int       PyEntity_set_scale(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_rotation(PyEntityObject *self, void *closure);
static int       PyEntity_set_rotation(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_selectable(PyEntityObject *self, void *closure);
static int       PyEntity_set_selectable(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_selection_radius(PyEntityObject *self, void *closure);
static int       PyEntity_set_selection_radius(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_get_pfobj_path(PyEntityObject *self, void *closure);
static PyObject *PyEntity_get_speed(PyEntityObject *self, void *closure);
static int       PyEntity_set_speed(PyEntityObject *self, PyObject *value, void *closure);
static PyObject *PyEntity_activate(PyEntityObject *self);
static PyObject *PyEntity_deactivate(PyEntityObject *self);
static PyObject *PyEntity_register(PyEntityObject *self, PyObject *args);
static PyObject *PyEntity_unregister(PyEntityObject *self, PyObject *args);
static PyObject *PyEntity_notify(PyEntityObject *self, PyObject *args);
static PyObject *PyEntity_select(PyEntityObject *self);
static PyObject *PyEntity_deselect(PyEntityObject *self);

static int       PyAnimEntity_init(PyAnimEntityObject *self, PyObject *args, PyObject *kwds);
static PyObject *PyAnimEntity_play_anim(PyAnimEntityObject *self, PyObject *args);

/*****************************************************************************/
/* STATIC VARIABLES                                                          */
/*****************************************************************************/

static PyMethodDef PyEntity_methods[] = {
    {"activate", 
    (PyCFunction)PyEntity_activate, METH_NOARGS,
    "Add the entity to the game world, making it visible and allowing other entities "
    "to interact with it in the simulation. The activated entity will be removed from "
    "the game world when no more references to it remain in scope. (ex: Using 'del' "
    "when you have a single reference)"},

    {"deactivate", 
    (PyCFunction)PyEntity_deactivate, METH_NOARGS,
    "Remove the entity from the game simulation and hiding it. The entity's state is "
    "preserved until it is activated again."},

    {"register", 
    (PyCFunction)PyEntity_register, METH_VARARGS,
    "Registers the specified callable to be invoked when an event of the specified type "
    "is sent to this entity." },

    {"unregister", 
    (PyCFunction)PyEntity_unregister, METH_VARARGS,
    "Unregisters a callable previously registered to be invoked on the specified event."},

    {"notify", 
    (PyCFunction)PyEntity_notify, METH_VARARGS,
    "Send a specific event to an entity in order to invoke the entity's event handlers."},

    {"select", 
    (PyCFunction)PyEntity_select, METH_NOARGS,
    "Adds the entity to the current unit selection, if it is not present there already."},

    {"deselect", 
    (PyCFunction)PyEntity_deselect, METH_NOARGS,
    "Removes the entity from the current unit selection, if it is selected."},

    {NULL}  /* Sentinel */
};

static PyGetSetDef PyEntity_getset[] = {
    {"name",
    (getter)PyEntity_get_name, (setter)PyEntity_set_name,
    "Custom name given to this enity.",
    NULL},
    {"pos",
    (getter)PyEntity_get_pos, (setter)PyEntity_set_pos,
    "The XYZ position in worldspace coordinates.",
    NULL},
    {"scale",
    (getter)PyEntity_get_scale, (setter)PyEntity_set_scale,
    "The XYZ scaling factors.",
    NULL},
    {"rotation",
    (getter)PyEntity_get_rotation, (setter)PyEntity_set_rotation,
    "XYZW quaternion for rotaion about local origin.",
    NULL},
    {"selectable",
    (getter)PyEntity_get_selectable, (setter)PyEntity_set_selectable,
    "Flag indicating whether this entity can be selected with the mouse.",
    NULL},
    {"selection_radius",
    (getter)PyEntity_get_selection_radius, (setter)PyEntity_set_selection_radius,
    "Radius (in OpenGL coordinates) of the unit selection circle for this entity.",
    NULL},
    {"pfobj_path",
    (getter)PyEntity_get_pfobj_path, NULL,
    "The relative path of the PFOBJ file used to instantiate the entity. Readonly.",
    NULL},
    {"speed",
    (getter)PyEntity_get_speed, (setter)PyEntity_set_speed,
    "Entity's movement speed (in OpenGL coordinates per second).",
    NULL},
    {NULL}  /* Sentinel */
};

static PyMethodDef PyAnimEntity_methods[] = {
    {"play_anim", 
    (PyCFunction)PyAnimEntity_play_anim, METH_VARARGS,
    "Play the animation clip with the specified name." },
    {NULL}  /* Sentinel */
};


static PyTypeObject PyEntity_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pf.Entity",               /* tp_name */
    sizeof(PyEntityObject),    /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)PyEntity_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Permafrost Engine generic game entity.", /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    PyEntity_methods,          /* tp_methods */
    0,                         /* tp_members */
    PyEntity_getset,           /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    PyEntity_new,              /* tp_new */
};

static PyTypeObject PyAnimEntity_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "pf.AnimEntity",
    .tp_basicsize = sizeof(PyAnimEntityObject), 
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "Permafrost Engine animated entity.",
    .tp_methods   = PyAnimEntity_methods,
    .tp_base      = &PyEntity_type,
    .tp_init      = (initproc)PyAnimEntity_init,
};

KHASH_MAP_INIT_INT(PyObject, PyObject*)
static khash_t(PyObject) *s_uid_pyobj_table;

/*****************************************************************************/
/* STATIC FUNCTIONS                                                          */
/*****************************************************************************/

static PyObject *PyEntity_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyEntityObject *self;
    const char *dirpath, *filename, *name;

    /* First, extract the first 3 args to handle the cases where subclasses get 
     * intialized with more arguments */
    PyObject *first_args = PyTuple_GetSlice(args, 0, 3);
    if(!first_args)
        return NULL;

    if(!PyArg_ParseTuple(first_args, "sss", &dirpath, &filename, &name)) {
        return NULL;
    }
    Py_DECREF(first_args);

    extern const char *g_basepath;
    char entity_path[512];
    strcpy(entity_path, g_basepath);
    strcat(entity_path, dirpath);

    struct entity *ent = AL_EntityFromPFObj(entity_path, filename, name);
    if(!ent) {
        return NULL;
    }

    self = (PyEntityObject*)type->tp_alloc(type, 0);
    if(!self) {
        AL_EntityFree(ent); 
        return NULL;
    }else{
        self->ent = ent; 
    }

    int ret;
    khiter_t k = kh_put(PyObject, s_uid_pyobj_table, ent->uid, &ret);
    assert(ret != -1 && ret != 0);
    kh_value(s_uid_pyobj_table, k) = (PyObject*)self;

    return (PyObject*)self;
}

static void PyEntity_dealloc(PyEntityObject *self)
{
    assert(self->ent);

    khiter_t k = kh_get(PyObject, s_uid_pyobj_table, self->ent->uid);
    assert(k != kh_end(s_uid_pyobj_table));
    kh_del(PyObject, s_uid_pyobj_table, k);

    G_RemoveEntity(self->ent);
    AL_EntityFree(self->ent);

    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *PyEntity_get_name(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("s", self->ent->name);
}

static int PyEntity_set_name(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyObject_IsInstance(value, (PyObject*)&PyString_Type)){
        PyErr_SetString(PyExc_TypeError, "Argument must be a string.");
        return -1;
    }

    const char *s = PyString_AsString(value);
    if(strlen(s) >= sizeof(self->ent->name)){
        PyErr_SetString(PyExc_TypeError, "Name string is too long.");
        return -1;
    }

    strcpy(self->ent->name, s);
    return 0;
}

static PyObject *PyEntity_get_pos(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("[f,f,f]", self->ent->pos.x, self->ent->pos.y, self->ent->pos.z);
}

static int PyEntity_set_pos(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyList_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a list.");
        return -1;
    }
    
    Py_ssize_t len = PyList_Size(value);
    if(len != 3) {
        PyErr_SetString(PyExc_TypeError, "Argument must have a size of 3."); 
        return -1;
    }

    for(int i = 0; i < len; i++) {

        PyObject *item = PyList_GetItem(value, i);
        if(!PyFloat_Check(item)) {
            PyErr_SetString(PyExc_TypeError, "List items must be floats.");
            return -1;
        }

        self->ent->pos.raw[i] = PyFloat_AsDouble(item);
    }

    return 0;
}

static PyObject *PyEntity_get_scale(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("[f,f,f]", self->ent->scale.x, self->ent->scale.y, self->ent->scale.z);
}

static int PyEntity_set_scale(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyList_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a list.");
        return -1;
    }
    
    Py_ssize_t len = PyList_Size(value);
    if(len != 3) {
        PyErr_SetString(PyExc_TypeError, "Argument must have a size of 3."); 
        return -1;
    }

    for(int i = 0; i < len; i++) {

        PyObject *item = PyList_GetItem(value, i);
        if(!PyFloat_Check(item)) {
            PyErr_SetString(PyExc_TypeError, "List items must be floats.");
            return -1;
        }

        self->ent->scale.raw[i] = PyFloat_AsDouble(item);
    }

    return 0;
}

static PyObject *PyEntity_get_rotation(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("[f,f,f,f]", self->ent->rotation.x, self->ent->rotation.y, 
        self->ent->rotation.z, self->ent->rotation.w);
}

static int PyEntity_set_rotation(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyList_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a list.");
        return -1;
    }
    
    Py_ssize_t len = PyList_Size(value);
    if(len != 4) {
        PyErr_SetString(PyExc_TypeError, "Argument must have a size of 4."); 
        return -1;
    }

    for(int i = 0; i < len; i++) {

        PyObject *item = PyList_GetItem(value, i);
        if(!PyFloat_Check(item)) {
            PyErr_SetString(PyExc_TypeError, "List items must be floats.");
            return -1;
        }

        self->ent->rotation.raw[i] = PyFloat_AsDouble(item);
    }

    return 0;
}

static PyObject *PyEntity_get_selectable(PyEntityObject *self, void *closure)
{
    if(self->ent->flags & ENTITY_FLAG_SELECTABLE)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static int PyEntity_set_selectable(PyEntityObject *self, PyObject *value, void *closure)
{
    int result = PyObject_IsTrue(value);

    if(-1 == result) {
        PyErr_SetString(PyExc_TypeError, "Argument must evaluate to True or False.");
        return -1;
    }else if(1 == result) {
        self->ent->flags |= ENTITY_FLAG_SELECTABLE;
    }else {
        self->ent->flags &= ~ENTITY_FLAG_SELECTABLE;
    }

    return 0;
}

static PyObject *PyEntity_get_selection_radius(PyEntityObject *self, void *closure)
{
    return Py_BuildValue("f", self->ent->selection_radius);
}

static PyObject *PyEntity_get_pfobj_path(PyEntityObject *self, void *closure)
{
    char buff[sizeof(self->ent->basedir) + sizeof(self->ent->filename) + 2];
    strcpy(buff, self->ent->basedir);
    strcat(buff, "/");
    strcat(buff, self->ent->filename);
    return PyString_FromString(buff); 
}

static PyObject *PyEntity_get_speed(PyEntityObject *self, void *closure)
{
    return PyFloat_FromDouble(self->ent->max_speed);
}

static int PyEntity_set_speed(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyFloat_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Speed attribute must be a float.");
        return -1;
    }

    self->ent->max_speed = PyFloat_AS_DOUBLE(value);
    return 0;
}

static int PyEntity_set_selection_radius(PyEntityObject *self, PyObject *value, void *closure)
{
    if(!PyFloat_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a float.");
        return -1;
    }

    self->ent->selection_radius = PyFloat_AsDouble(value);
    return 0;
}

static PyObject *PyEntity_activate(PyEntityObject *self)
{
    assert(self->ent);
    G_AddEntity(self->ent);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_deactivate(PyEntityObject *self)
{
    assert(self->ent);
    G_RemoveEntity(self->ent);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_register(PyEntityObject *self, PyObject *args)
{
    enum eventtype event;
    PyObject *callable, *user_arg;

    if(!PyArg_ParseTuple(args, "iOO", &event, &callable, &user_arg)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be an integer and two objects.");
        return NULL;
    }

    if(!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "Second argument must be callable.");
        return NULL;
    }

    Py_INCREF(callable);
    Py_INCREF(user_arg);

    bool ret = E_Entity_ScriptRegister(event, self->ent->uid, callable, user_arg);
    assert(ret == true);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_unregister(PyEntityObject *self, PyObject *args)
{
    enum eventtype event;
    PyObject *callable;

    if(!PyArg_ParseTuple(args, "iO", &event, &callable)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must an integer and one object.");
        return NULL;
    }

    if(!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "Second argument must be callable.");
        return NULL;
    }

    bool ret = E_Entity_ScriptUnregister(event, self->ent->uid, callable);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_notify(PyEntityObject *self, PyObject *args)
{
    enum eventtype event;
    PyObject *arg;

    if(!PyArg_ParseTuple(args, "iO", &event, &arg)) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be an integer and one object.");
        return NULL;
    }

    Py_INCREF(arg);

    E_Entity_Notify(event, self->ent->uid, arg, ES_SCRIPT);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_select(PyEntityObject *self)
{
    assert(self->ent);
    G_Sel_Add(self->ent);
    Py_RETURN_NONE;
}

static PyObject *PyEntity_deselect(PyEntityObject *self)
{
    assert(self->ent);
    G_Sel_Remove(self->ent);
    Py_RETURN_NONE;
}

static int PyAnimEntity_init(PyAnimEntityObject *self, PyObject *args, PyObject *kwds)
{
    const char *dirpath, *filename, *name, *clipname;
    PyObject *first_args = PyTuple_GetSlice(args, 0, 4);
    if(!first_args)
        return -1;

    if(!PyArg_ParseTuple(first_args, "ssss", &dirpath, &filename, &name, &clipname)) {
        return -1;
    }
    Py_DECREF(first_args);

    A_InitCtx(self->super.ent, clipname, 24);
    return 0;
}

static PyObject *PyAnimEntity_play_anim(PyAnimEntityObject *self, PyObject *args)
{
    const char *clipname;
    if(!PyArg_ParseTuple(args, "s", &clipname)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a string.");
        return NULL;
    }

    A_SetActiveClip(self->super.ent, clipname, ANIM_MODE_LOOP, 24);
    Py_RETURN_NONE;
}

static PyObject *s_obj_from_attr(const struct attr *attr)
{
    switch(attr->type){
    case TYPE_STRING:   return Py_BuildValue("s", attr->val.as_string);
    case TYPE_FLOAT:    return Py_BuildValue("f", attr->val.as_float);
    case TYPE_INT:      return Py_BuildValue("i", attr->val.as_int);
    case TYPE_VEC3:     return Py_BuildValue("[f,f,f]", attr->val.as_vec3.x, attr->val.as_vec3.y, attr->val.as_vec3.z);
    case TYPE_QUAT:     return Py_BuildValue("[f,f,f,f]", 
                               attr->val.as_quat.x, attr->val.as_quat.y, attr->val.as_quat.z, attr->val.as_quat.w);
    case TYPE_BOOL:     return Py_BuildValue("i", attr->val.as_bool);
    default: assert(0);
    }
}

static PyObject *s_tuple_from_attr_vec(const kvec_attr_t *attr_vec)
{
    PyObject *ret = PyTuple_New(kv_size(*attr_vec));
    if(!ret)
        return NULL;

    for(int i = 0; i < kv_size(*attr_vec); i++) {
        PyTuple_SetItem(ret, i, s_obj_from_attr(&kv_A(*attr_vec, i)));
    }

    return ret;
}

static PyObject *s_entity_from_atts(const char *path, const char *name, const khash_t(attr) *attr_table)
{
    PyObject *ret;

    char copy[256];
    if(strlen(path) >= sizeof(copy)) 
        return NULL;
    strcpy(copy, path);
    char *end = copy + strlen(path);
    assert(*end == '\0');
    while(end > copy && *end != '/')
        end--;
    if(end == copy)
        return NULL;
    *end = '\0';
    const char *filename = end + 1;

    khiter_t k = kh_get(attr, attr_table, "animated");
    if(k == kh_end(attr_table))
        return NULL;
    bool anim = kh_value(attr_table, k).val.as_bool;

    PyObject *args = PyTuple_New(anim ? 4 : 3);
    if(!args)
        return NULL;

    PyTuple_SetItem(args, 0, PyString_FromString(copy));
    PyTuple_SetItem(args, 1, PyString_FromString(filename));
    PyTuple_SetItem(args, 2, PyString_FromString(name));

    if(anim) {
        k = kh_get(attr, attr_table, "idle_clip");
        if(k == kh_end(attr_table)) {
            Py_DECREF(args);
            return NULL;
        }
        PyTuple_SetItem(args, 3, s_obj_from_attr(&kh_value(attr_table, k)));
        ret = PyObject_CallObject((PyObject*)&PyAnimEntity_type, args);
    }else{
        ret = PyObject_CallObject((PyObject*)&PyEntity_type, args);
    }

    Py_DECREF(args);
    return ret;
}

static PyObject *s_new_custom_class(const char *name, const kvec_attr_t *construct_args)
{
    PyObject *sys_mod_dict = PyImport_GetModuleDict();
    PyObject *modules = PyMapping_Values(sys_mod_dict);
    PyObject *class = NULL;
    for(int i = 0; i < PyList_Size(modules); i++) {
        if(PyObject_HasAttrString(PyList_GetItem(modules, i), name)) {
            class = PyObject_GetAttrString(PyList_GetItem(modules, i), name);
            break;
        }
    }
    Py_DECREF(modules);
    if(!class)
        return NULL;

    PyObject *args = s_tuple_from_attr_vec(construct_args);
    if(!args){
        Py_DECREF(class);
        return NULL;
    }
    PyObject *ret = PyObject_CallObject(class, args);
    if(!ret) {
        PyErr_Print();
        exit(EXIT_FAILURE);
    }

    Py_DECREF(class);
    Py_DECREF(args);

    return ret;
}

/*****************************************************************************/
/* EXTERN FUNCTIONS                                                          */
/*****************************************************************************/

void S_Entity_PyRegister(PyObject *module)
{
    if(PyType_Ready(&PyEntity_type) < 0)
        return;
    Py_INCREF(&PyEntity_type);
    PyModule_AddObject(module, "Entity", (PyObject*)&PyEntity_type);

    if(PyType_Ready(&PyAnimEntity_type) < 0)
        return;
    Py_INCREF(&PyAnimEntity_type);
    PyModule_AddObject(module, "AnimEntity", (PyObject*)&PyAnimEntity_type);
}

bool S_Entity_Init(void)
{
    s_uid_pyobj_table = kh_init(PyObject);
    return (s_uid_pyobj_table != NULL);
}

void S_Entity_Shutdown(void)
{
    kh_destroy(PyObject, s_uid_pyobj_table);
}

PyObject *S_Entity_ObjForUID(uint32_t uid)
{
    khiter_t k = kh_get(PyObject, s_uid_pyobj_table, uid);
    if(k == kh_end(s_uid_pyobj_table))
        return NULL;

    return kh_value(s_uid_pyobj_table, k);
}

script_opaque_t S_Entity_ObjFromAtts(const char *path, const char *name,
                                     const khash_t(attr) *attr_table, 
                                     const kvec_attr_t *construct_args)
{
    khiter_t k;
    PyObject *ret = NULL;

    /* First, attempt to make an instance of the custom class specified in the 
     * attributes dictionary, if the key is present. */
    if((k = kh_get(attr, attr_table, "class")) != kh_end(attr_table)) {
        ret = s_new_custom_class(kh_value(attr_table, k).val.as_string, construct_args);
    }

    /* If we could not make a custom class, fall back to instantiating a basic entity */
    if(!ret){
        ret = s_entity_from_atts(path, name, attr_table); 
    }

    if(!ret){
        return NULL;
    }

    if((k = kh_get(attr, attr_table, "static")) != kh_end(attr_table)) {
        if(kh_value(attr_table, k).val.as_bool)
            ((PyEntityObject*)ret)->ent->flags |= ENTITY_FLAG_STATIC;
        else
            ((PyEntityObject*)ret)->ent->flags &= ~ENTITY_FLAG_STATIC;
    }

    if((k = kh_get(attr, attr_table, "collision")) != kh_end(attr_table)) {
        if(kh_value(attr_table, k).val.as_bool)
            ((PyEntityObject*)ret)->ent->flags |= ENTITY_FLAG_COLLISION;
        else
            ((PyEntityObject*)ret)->ent->flags &= ~ENTITY_FLAG_COLLISION;
    }

    if(PyObject_HasAttrString(ret, "pos") 
    && (k = kh_get(attr, attr_table, "position")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "pos", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "scale") 
    && (k = kh_get(attr, attr_table, "scale")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "scale", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "rotation") 
    && (k = kh_get(attr, attr_table, "rotation")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "rotation", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "selectable") 
    && (k = kh_get(attr, attr_table, "selectable")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "selectable", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "selection_radius") 
    && (k = kh_get(attr, attr_table, "selection_radius")) != kh_end(attr_table))
        PyObject_SetAttrString(ret, "selection_radius", s_obj_from_attr(&kh_value(attr_table, k)));

    if(PyObject_HasAttrString(ret, "activate")) {
        PyObject *result = PyObject_CallMethod(ret, "activate", "");
        Py_XDECREF(result);
        if(!result) {
            PyErr_Print();
            exit(EXIT_FAILURE);
        }
    }

    return ret;
}

PyObject *S_Entity_GetAllList(void)
{
    PyObject *ret = PyList_New(kh_size(s_uid_pyobj_table));
    if(!ret)
        return NULL;

    uint32_t key;
    PyObject *curr;
    int i = 0;
    kh_foreach(s_uid_pyobj_table, key, curr, {
        PyList_SetItem(ret, i++, curr); /* steals reference */
    });
    
    return ret;
}

