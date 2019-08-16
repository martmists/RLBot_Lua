//
// Created by martmists on 8/15/19.
//

extern "C" {
    #include <Python.h>
    #include <cstdio>
    #include <cstring>
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <iostream>

int PyModule_AddType(PyObject *module, const char *name, PyTypeObject *type) {
    if (PyType_Ready(type)) {
        return -1;
    }
    Py_INCREF(type);
    if (PyModule_AddObject(module, name, (PyObject *)type)) {
        Py_DECREF(type);
        return -1;
    }
    return 0;
}

void run_file(lua_State *L, char* filename, int ret){
    if (luaL_loadfile(L, filename) || lua_pcall(L, 0, ret, 0))
        luaL_error(L, "cannot load required file: %s", lua_tostring(L, -1));
}

struct LuaAgent {
    PyObject_HEAD
    lua_State *L;
};

lua_State* createAgent(int index){
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // Add _G to the stack
    lua_getglobal(L, "_G");

    // Register `class` and `super`
    run_file(L, (char*)"classes.lua", 2);
    lua_setfield(L, 1, "dump");
    lua_setfield(L, 1, "super");
    lua_setfield(L, 1, "class");

    // Register structs
    run_file(L, (char*)"structs.lua", 0);

    // Load bot onto the stack
    // THIS FILE MUST RETURN AN INSTANCE OR IT WONT WORK!
    run_file(L, (char*)"bot.lua", 1);

    // Call bot_init
    lua_pushstring(L, "bot_init");
    lua_pushnumber(L, index+1);
    lua_call(L, 1, 0);

    // Pop _G from the stack
    lua_pop(L, 1);

    return L;
}

void getInt(lua_State* L, PyObject* parent, char* name){
    PyObject* prop = PyObject_GetAttrString(parent, name);
    long x = PyLong_AsLong(prop);
    Py_DECREF(prop);
    lua_pushnumber(L, x);
    lua_setfield(L, -2, name);
}

void getDouble(lua_State* L, PyObject* parent, char* name){
    PyObject* prop = PyObject_GetAttrString(parent, name);
    double x = PyFloat_AsDouble(prop);
    Py_DECREF(prop);
    lua_pushnumber(L, x);
    lua_setfield(L, -2, name);
}

void getBool(lua_State* L, PyObject* parent, char* name){
    PyObject* prop = PyObject_GetAttrString(parent, name);
    bool x = PyObject_IsTrue(prop);
    Py_DECREF(prop);
    lua_pushboolean(L, x);
    lua_setfield(L, -2, name);
}

void getString(lua_State* L, PyObject* parent, char* name){
    PyObject* prop = PyObject_GetAttrString(parent, name);
    char* x = PyBytes_AsString(prop);
    Py_DECREF(prop);
    lua_pushstring(L, x);
    lua_setfield(L, -2, name);
}

void getVector(lua_State* L, PyObject* parent, char* name){
    lua_newtable(L);
    PyObject* vec = PyObject_GetAttrString(parent, name);
    getDouble(L, vec, (char*)"x");
    getDouble(L, vec, (char*)"y");
    getDouble(L, vec, (char*)"z");
    Py_DECREF(vec);
    lua_setfield(L, -2, name);
}

void getRotation(lua_State* L, PyObject* parent, char* name){
    lua_newtable(L);
    PyObject* vec = PyObject_GetAttrString(parent, name);
    getDouble(L, vec, (char*)"pitch");
    getDouble(L, vec, (char*)"yaw");
    getDouble(L, vec, (char*)"roll");
    Py_DECREF(vec);
    lua_setfield(L, -2, name);
}

void createLuaPacket(lua_State *L, PyObject* packet){
    // Get _G for the classes
    // stack: [Bot, "get_output"]
    lua_getglobal(L, "_G");
    // stack: [Bot, "get_output", _G]
    lua_pushstring(L, "GameTickPacket");
    // stack: [Bot, "get_output", _G, 'GameTickPacket']
    lua_newtable(L);

    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}]
    PyObject* num_cars = PyObject_GetAttrString(packet, "num_cars");
    long num_cars_i = PyLong_AsLong(num_cars);
    lua_pushinteger(L, num_cars_i);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {int num_cars}]
    lua_setfield(L, -2, "num_cars");
    Py_DECREF(num_cars);

    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}]
    PyObject* cars = PyObject_GetAttrString(packet, "game_cars");
    lua_newtable(L);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}]
    for (long i = 0; i < num_cars_i; i++){
        // Create car
        PyObject* index = PyLong_FromLong(i);
        PyObject* car = PyObject_GetItem(cars, index);
        Py_DECREF(index);
        lua_newtable(L);

        // Physics
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}, {table car_n}]
        PyObject* physics = PyObject_GetAttrString(car, "physics");
        lua_newtable(L);
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}, {table car_n}, {table car_n_physics}]
        char* physics_keys[] = {(char*)"location", (char*)"velocity", (char*)"angular_velocity", (char*)"rotation"};

        for (char* physics_key : physics_keys) {
            if (strcmp(physics_key, "rotation") != 0){
                getRotation(L, physics, physics_key);
            } else {
                getVector(L, physics, physics_key);
            }
        }
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}, {table car_n}, {table car_n_physics}]
        lua_setfield(L, -2, "physics");
        Py_DECREF(physics);

        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}, {table car_n}]
        char* bool_keys[] = {
                (char*)"is_demolished",
                (char*)"has_wheel_contact",
                (char*)"is_super_sonic",
                (char*)"is_bot",
                (char*)"jumped",
                (char*)"double_jumped"
        };
        for (char* key : bool_keys){
            getBool(L, car, key);
        }

        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}, {table car_n}]
        getString(L, car, (char*)"name");
        getInt(L, car, (char*)"team");
        getDouble(L, car, (char*)"boost");

        lua_newtable(L);
        PyObject* hitbox = PyObject_GetAttrString(car, "hitbox");
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}, {table car_n}, {table hitbox}]
        getInt(L, hitbox, (char*)"length");
        getInt(L, hitbox, (char*)"width");
        getInt(L, hitbox, (char*)"height");

        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}, {table car_n}, {table hitbox}]
        lua_setfield(L, -2, "hitbox");
        Py_DECREF(hitbox);

        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}, {table car_n}]
        Py_DECREF(car);
        lua_rawseti(L, -2, i+1);
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}]
    }
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_cars}]
    lua_setfield(L, -2, "game_cars");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}]

    PyObject* num_boost = PyObject_GetAttrString(packet, "num_boost");
    long num_boost_i = PyLong_AsLong(num_boost);
    lua_pushinteger(L, num_boost_i);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {int num_boost}]
    lua_setfield(L, -2, "num_boost");
    Py_DECREF(num_boost);

    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}]
    lua_newtable(L);
    PyObject* boosts = PyObject_GetAttrString(packet, (char*)"game_boosts");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table boosts}]
    for (long i = 0; i < num_boost_i; i++) {
        PyObject* index = PyLong_FromLong(i);
        PyObject* boost_obj = PyObject_GetItem(boosts, index);
        Py_DECREF(index);
        lua_newtable(L);
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table boosts}, {table boost_<i>}]
        getBool(L, boost_obj, (char*)"is_active");
        getDouble(L, boost_obj, (char*)"timer");
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table boosts}, {table boost_<i>}]
        lua_rawseti(L, -2, i+1);
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table boosts}]
        Py_DECREF(boost_obj);
    }
    Py_DECREF(boosts);

    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table boosts}]
    lua_setfield(L, -2, "game_boosts");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}]

    lua_newtable(L);
    PyObject* ball = PyObject_GetAttrString(packet, (char*)"game_ball");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}]
    PyObject* physics = PyObject_GetAttrString(ball, "physics");
    lua_newtable(L);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table physics}]
    char* physics_keys[] = {(char*)"location", (char*)"velocity", (char*)"angular_velocity", (char*)"rotation"};

    for (char* physics_key : physics_keys) {
        if (strcmp(physics_key, "rotation") != 0){
            getRotation(L, physics, physics_key);
        } else {
            getVector(L, physics, physics_key);
        }
    }
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table physics}]
    lua_setfield(L, -2, "physics");
    Py_DECREF(physics);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}]

    lua_newtable(L);
    PyObject* last_touch = PyObject_GetAttrString(ball, "latest_touch");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table last_touch}]
    getString(L, last_touch, (char*)"player_name");
    getDouble(L, last_touch, (char*)"time_seconds");
    getInt(L, last_touch, (char*)"team");
    getInt(L, last_touch, (char*)"player_index");
    getVector(L, last_touch, (char*)"hit_location");
    getVector(L, last_touch, (char*)"hit_normal");

    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table last_touch}]
    Py_DECREF(last_touch);
    lua_setfield(L, -2, "latest_touch");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}]

    lua_newtable(L);
    PyObject* dropshot_info = PyObject_GetAttrString(ball, "drop_shot_info");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table dropshot}]
    getInt(L, dropshot_info, (char*)"damage_index");
    getInt(L, dropshot_info, (char*)"absorbed_force");
    getInt(L, dropshot_info, (char*)"force_accum_recent");
    Py_DECREF(dropshot_info);
    lua_setfield(L, -2, "drop_shot_info");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}]

    lua_newtable(L);
    PyObject* collision = PyObject_GetAttrString(ball, "collision_shape");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table collision}]
    getInt(L, collision, (char*)"type");

    lua_newtable(L);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table collision}, {table box}]
    PyObject* box = PyObject_GetAttrString(collision, (char*)"box");
    getDouble(L, box, (char*)"length");
    getDouble(L, box, (char*)"width");
    getDouble(L, box, (char*)"height");
    Py_DECREF(box);
    lua_setfield(L, -2, "box");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table collision}]

    lua_newtable(L);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table collision}, {table sphere}]
    PyObject* sphere = PyObject_GetAttrString(collision, (char*)"sphere");
    getDouble(L, sphere, (char*)"diameter");
    Py_DECREF(sphere);
    lua_setfield(L, -2, "sphere");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table collision}]

    lua_newtable(L);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table collision}, {table cylinder}]
    PyObject* cylinder = PyObject_GetAttrString(collision, (char*)"cylinder");
    getDouble(L, cylinder, (char*)"diameter");
    getDouble(L, cylinder, (char*)"height");
    Py_DECREF(cylinder);
    lua_setfield(L, -2, "cylinder");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}, {table collision}]
    Py_DECREF(collision);
    lua_setfield(L, -2, "collision_shape");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table ball}]

    lua_setfield(L, -2, "game_ball");
    Py_DECREF(ball);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}]

    lua_newtable(L);
    PyObject* game_info = PyObject_GetAttrString(packet, (char*)"game_info");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table game_info}]
    getDouble(L, game_info, (char*)"seconds_elapsed");
    getDouble(L, game_info, (char*)"game_time_remaining");
    getDouble(L, game_info, (char*)"world_gravity_z");
    getDouble(L, game_info, (char*)"game_speed");
    getBool(L, game_info, (char*)"is_overtime");
    getBool(L, game_info, (char*)"is_unlimited_time");
    getBool(L, game_info, (char*)"is_round_active");
    getBool(L, game_info, (char*)"is_kickoff_pause");
    getBool(L, game_info, (char*)"is_match_ended");

    Py_DECREF(game_info);
    lua_setfield(L, -2, "game_info");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}]

    PyObject* num_teams = PyObject_GetAttrString(packet, "num_teams");
    long num_teams_i = PyLong_AsLong(num_cars);
    lua_pushinteger(L, num_teams_i);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {int num_teams}]
    lua_setfield(L, -2, "num_teams");
    Py_DECREF(num_teams);

    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}]
    lua_newtable(L);
    PyObject* teams = PyObject_GetAttrString(packet, "teams");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table teams}]

    for (long i = 0; i < num_teams_i; i++){
        PyObject* index = PyLong_FromLong(i);
        PyObject* team = PyObject_GetItem(teams, index);
        Py_DECREF(index);
        lua_newtable(L);
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table teams}, {table team_n}]

        getInt(L, team, (char*)"team_index");
        getInt(L, team, (char*)"score");
        lua_rawseti(L, -2, i+1);
        // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table teams}]
        Py_DECREF(team);
    }
    Py_DECREF(teams);
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}, {table teams}]
    lua_setfield(L, -2, "teams");
    // stack: [Bot, "get_output", _G, 'GameTickPacket', {table packet}]

    // Packet is now on top the stack

    lua_call(L, 1, 1);
    // stack: [Bot, "get_output", _G, <class GameTickPacket>]

    // Remove _G again
    lua_pop(L, -2);
    // stack: [Bot, "get_output", <class GameTickPacket>]
}

PyObject* runAgent(LuaAgent* agent, PyObject* packet) {
    lua_State *L = agent->L;

    // Stack: [Bot]
    // Create function name
    lua_pushstring(L, "get_output");

    // Parse and prepare packet
    createLuaPacket(L, packet);
    // stack: [Bot, "get_output", <class GameTickPacket>]

    // Call function, puts controller state to the stack
    lua_call(L, 1, 1);
    // stack: [Bot, <class ControllerState>]

    // Get properties from controller state
    lua_getfield(L, -1, "steer");
    double steer = lua_tonumber(L, -1);
    lua_getfield(L, -1, "throttle");
    double throttle = lua_tonumber(L, -1);
    lua_getfield(L, -1, "pitch");
    double pitch = lua_tonumber(L, -1);
    lua_getfield(L, -1, "yaw");
    double yaw = lua_tonumber(L, -1);
    lua_getfield(L, -1, "roll");
    double roll = lua_tonumber(L, -1);
    lua_getfield(L, -1, "jump");
    bool jump = lua_toboolean(L, -1);
    lua_getfield(L, -1, "boost");
    bool boost = lua_toboolean(L, -1);
    lua_getfield(L, -1, "handbrake");
    bool handbrake = lua_toboolean(L, -1);
    lua_getfield(L, -1, "use_item");
    bool use_item = lua_toboolean(L, -1);

    PyObject* ret = Py_BuildValue("dddddbbbb", steer, throttle, pitch, yaw, roll, jump, boost, handbrake, use_item);

    // Pop controller state
    lua_pop(L, -1);
    // stack: [Bot]

    return ret;
}

static int Agent_tp_init(PyObject *_self, PyObject *args, PyObject *kwargs) {
    auto* self = (LuaAgent*)_self;

    int index;
    char* kwlist[] = {(char*)"index", nullptr};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i:__init__", kwlist, &index)) {
        return -1;
    }

    self->L = createAgent(index);
    return 0;
}

PyObject* Agent_GetOutput(PyObject *_self, PyObject *args, PyObject *kwargs){
    auto* self = (LuaAgent*)_self;

    PyObject* packet = nullptr;
    char* kwlist[] = {(char*)"packet", nullptr};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:__init__", kwlist, &packet)) {
        PyErr_SetString(PyExc_ValueError, "Unable to call GetOutput");
        return nullptr;
    }

    return runAgent(self, packet);
}

static int Agent_tp_clear(PyObject *self) {
    return 0;
}

static void Agent_tp_dealloc(PyObject *self) {
    Agent_tp_clear(self);
    Py_TYPE(self)->tp_free(self);
}

PyMethodDef Agent_Methods[] = {
        {"get_output", (PyCFunction) Agent_GetOutput, METH_VARARGS, "Returns a controller state from a GTP"},
        {nullptr}
};

static PyTypeObject PyType_LuaBot = {
        PyVarObject_HEAD_INIT(nullptr, 0)
        .tp_name = "rlbot_lua.LuaBot",
        .tp_basicsize = sizeof(LuaAgent),
        .tp_itemsize = 0,
        .tp_dealloc = (destructor) Agent_tp_dealloc,
        .tp_flags = Py_TPFLAGS_DEFAULT,
        .tp_clear = (inquiry) Agent_tp_clear,
        .tp_methods = Agent_Methods,
        .tp_init = (initproc) Agent_tp_init,
        .tp_alloc = PyType_GenericAlloc,
        .tp_new = PyType_GenericNew
};

PyObject* RLBot_Lua__module = nullptr;

PyMODINIT_FUNC PyInit_rlbot_lua(){
    static struct PyModuleDef moduledef = {
            PyModuleDef_HEAD_INIT,
            "rlbot_lua",     /* m_name */
            "Lua bridge for rlbot",  /* m_doc */
            -1,                  /* m_size */
            nullptr,   /* m_methods */
            nullptr,                /* m_reload */
            nullptr,                /* m_traverse */
            nullptr,                /* m_clear */
            nullptr,                /* m_free */
    };

    RLBot_Lua__module = PyModule_Create(&moduledef);

    if (RLBot_Lua__module == nullptr){
        std::cerr << "Error loading RLBot Lua-C module.";
        return nullptr;
    }

    PyModule_AddType(RLBot_Lua__module, "LuaBot", &PyType_LuaBot);

    return RLBot_Lua__module;
}