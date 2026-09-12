#include <iostream>
#include <unordered_map>

using namespace std;

class Node {
  public:
    string key, val;
    Node *prev, *next;

    Node(string key, string val) {
        this->key = key;
        this->val = val;
        this->prev = nullptr;
        this->next = nullptr;
    }

    Node(string val, Node* prev, Node* next) {
        this->val = val;
        this->prev = prev;
        this->next = next;
    }
};

class LRUCache {
    int capacity;
    unordered_map<string, Node*> m;
    Node *last, *first;

public:
    LRUCache(int capacity) {
        this->capacity = capacity;
        last = nullptr;
        first = nullptr;
    }

    string get(string key) {
        auto it = m.find(key);
        if (it == m.end()) return "";
        else if (it->second == last) {
            return it->second->val;
        } else if (it->second == first) {
            Node* n = it->second;
            first = first->next;
            first->prev = nullptr;

            n->prev = last;
            last->next = n;
            n->next = nullptr;
            last = n;
            return n->val;
        } else {
            Node* n = it->second;
            n->prev->next = n->next;
            n->next->prev = n->prev;

            n->prev = last;
            last->next = n;
            n->next = nullptr;
            last = n;
            return n->val;
        }
    }

    void put(string key, string value) {
        auto it = m.find(key);
        if (it != m.end()) {
            it->second->val = value;
            get(key);
        } else {
            m[key] = new Node(key, value);
            if (!first) {
                first = m[key];
                last = m[key];
                return;
            }
            last->next = m[key];
            m[key]->prev = last;
            last = last->next;


            if (m.size() > capacity) {
                m.erase(first->key);
                first = first->next;
                first->prev = nullptr;
            }
        }
    }
};
