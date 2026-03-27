#pragma once
#include <iostream>
#include <mutex>
#include <condition_variable>

template <typename T>
class BufferQueue {
private:
    struct Node {
        T data;
        Node* next;
        Node(T val) : data(val), next(nullptr) {}
    };

    Node* head;
    Node* tail;
    size_t count;
    std::mutex mtx;
    std::condition_variable cv;

public:
    BufferQueue() : head(nullptr), tail(nullptr), count(0) {}

    ~BufferQueue() {
        while (head) {
            Node* temp = head;
            head = head->next;
            delete temp;
        }
    }

    void enqueue(T value) {
        Node* newNode = new Node(value);
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (!tail) {
                head = tail = newNode;
            }
            else {
                tail->next = newNode;
                tail = newNode;
            }
            count++;
        }
        cv.notify_one();
    }

    T dequeue() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return count > 0; });

        Node* temp = head;
        T value = temp->data;
        head = head->next;

        if (!head) {
            tail = nullptr;
        }

        delete temp;
        count--;
        return value;
    }

    T front() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!head) throw std::runtime_error("Queue is empty");
        return head->data;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }

    bool isEmpty() {
        std::lock_guard<std::mutex> lock(mtx);
        return count == 0;
    }
};
