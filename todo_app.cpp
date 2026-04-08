// ============================================================
//  TaskBoard — C++ Logic (compiled to WebAssembly via Emscripten)
//  Compile with:
//    emcc todo_app.cpp -o todo_app.js \
//         -s WASM=1 \
//         -s EXPORTED_FUNCTIONS='["_addTask","_deleteTask","_toggleTask","_editTask","_getTasks","_addNote","_deleteNote","_getNotes","_editNote","_getTaskCount","_getCompletedCount","_clearCompleted","_malloc","_free"]' \
//         -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8","lengthBytesUTF8"]' \
//         -s ALLOW_MEMORY_GROWTH=1 \
//         -O2
// ============================================================

#include <emscripten/emscripten.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdlib>

// ── Data structures ──────────────────────────────────────────

struct Task {
    int         id;
    std::string title;
    std::string category;
    std::string priority;   // "high" | "medium" | "low"
    bool        done;
    std::string created;    // ISO date string passed from JS
};

struct Note {
    int         id;
    std::string title;
    std::string body;
    std::string color;      // hex accent colour
    std::string created;
};

// ── In-memory stores ─────────────────────────────────────────

static std::vector<Task> g_tasks;
static std::vector<Note> g_notes;
static int g_nextTaskId = 1;
static int g_nextNoteId = 1;

// ── Helpers ──────────────────────────────────────────────────

// Escape double-quotes inside a string so it can be embedded in JSON
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

static std::string taskToJson(const Task& t) {
    std::ostringstream os;
    os << "{"
       << "\"id\":"       << t.id                      << ","
       << "\"title\":\""  << jsonEscape(t.title)        << "\","
       << "\"category\":\"" << jsonEscape(t.category)   << "\","
       << "\"priority\":\"" << jsonEscape(t.priority)   << "\","
       << "\"done\":"     << (t.done ? "true" : "false")<< ","
       << "\"created\":\"" << jsonEscape(t.created)     << "\""
       << "}";
    return os.str();
}

static std::string noteToJson(const Note& n) {
    std::ostringstream os;
    os << "{"
       << "\"id\":"       << n.id                      << ","
       << "\"title\":\""  << jsonEscape(n.title)        << "\","
       << "\"body\":\""   << jsonEscape(n.body)         << "\","
       << "\"color\":\""  << jsonEscape(n.color)        << "\","
       << "\"created\":\"" << jsonEscape(n.created)     << "\""
       << "}";
    return os.str();
}

// Return a heap-allocated C string the JS side must NOT free
// (we keep a static buffer per call — fine for single-threaded WASM)
static const char* returnString(const std::string& s) {
    static std::string buf;
    buf = s;
    return buf.c_str();
}

// ── Exported Task API ─────────────────────────────────────────

extern "C" {

EMSCRIPTEN_KEEPALIVE
int addTask(const char* title, const char* category,
            const char* priority, const char* created) {
    Task t;
    t.id       = g_nextTaskId++;
    t.title    = title    ? title    : "";
    t.category = category ? category : "General";
    t.priority = priority ? priority : "medium";
    t.done     = false;
    t.created  = created  ? created  : "";
    g_tasks.push_back(t);
    return t.id;
}

EMSCRIPTEN_KEEPALIVE
int deleteTask(int id) {
    for (auto it = g_tasks.begin(); it != g_tasks.end(); ++it) {
        if (it->id == id) { g_tasks.erase(it); return 1; }
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int toggleTask(int id) {
    for (auto& t : g_tasks) {
        if (t.id == id) { t.done = !t.done; return t.done ? 1 : 0; }
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
int editTask(int id, const char* title, const char* category,
             const char* priority) {
    for (auto& t : g_tasks) {
        if (t.id == id) {
            if (title)    t.title    = title;
            if (category) t.category = category;
            if (priority) t.priority = priority;
            return 1;
        }
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
const char* getTasks() {
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < g_tasks.size(); ++i) {
        if (i) os << ",";
        os << taskToJson(g_tasks[i]);
    }
    os << "]";
    return returnString(os.str());
}

EMSCRIPTEN_KEEPALIVE
int getTaskCount() {
    return (int)g_tasks.size();
}

EMSCRIPTEN_KEEPALIVE
int getCompletedCount() {
    int n = 0;
    for (auto& t : g_tasks) if (t.done) ++n;
    return n;
}

EMSCRIPTEN_KEEPALIVE
int clearCompleted() {
    int removed = 0;
    auto it = g_tasks.begin();
    while (it != g_tasks.end()) {
        if (it->done) { it = g_tasks.erase(it); ++removed; }
        else          { ++it; }
    }
    return removed;
}

// ── Exported Note API ─────────────────────────────────────────

EMSCRIPTEN_KEEPALIVE
int addNote(const char* title, const char* body,
            const char* color, const char* created) {
    Note n;
    n.id      = g_nextNoteId++;
    n.title   = title   ? title   : "Untitled";
    n.body    = body    ? body    : "";
    n.color   = color   ? color   : "#7c6dfa";
    n.created = created ? created : "";
    g_notes.push_back(n);
    return n.id;
}

EMSCRIPTEN_KEEPALIVE
int deleteNote(int id) {
    for (auto it = g_notes.begin(); it != g_notes.end(); ++it) {
        if (it->id == id) { g_notes.erase(it); return 1; }
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int editNote(int id, const char* title, const char* body, const char* color) {
    for (auto& n : g_notes) {
        if (n.id == id) {
            if (title) n.title = title;
            if (body)  n.body  = body;
            if (color) n.color = color;
            return 1;
        }
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
const char* getNotes() {
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < g_notes.size(); ++i) {
        if (i) os << ",";
        os << noteToJson(g_notes[i]);
    }
    os << "]";
    return returnString(os.str());
}

} // extern "C"
