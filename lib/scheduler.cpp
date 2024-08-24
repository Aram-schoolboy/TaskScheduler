#include <iostream>
#include <memory>
#include <vector>

class Any {
    template <typename T>
    friend T& GetValue(const Any& any);

private:
    struct Base {
        virtual Base* GetCopy() const = 0;

        virtual ~Base() = default;
    };

    template <typename T>
    struct Derived: public Base {
        T value;

        Derived(const T& value): value(value) {}

        Base* GetCopy() const override {
            return new Derived(value);
        }
    };

    Base* ptr_;

public:
    template <typename T>
    Any(const T& value): ptr_(new Derived<T>(value)) {}

    Any(const Any& other) {
        ptr_ = other.ptr_ == nullptr ? nullptr : other.ptr_->GetCopy();
    }

    template <typename T>
    Any& operator=(const T& value) {
        delete ptr_;
        ptr_ = new Derived<T>(value);
        return *this;
    }

    Any& operator=(const Any& other) {
        if (other.ptr_ != ptr_) {
            delete ptr_;
            ptr_ = other.ptr_ == nullptr ? nullptr : other.ptr_->GetCopy();
        }
        return *this;
    }

    Any(): ptr_(nullptr) {}

    ~Any() {
        delete ptr_;
    }

    bool HasValue() const {
        return ptr_ != nullptr;
    }
};

template <typename T>
T& GetValue(const Any& any) {
    Any::Derived<T>* p = dynamic_cast<Any::Derived<T>*>(any.ptr_);
    if (!p) {
        std::cerr << "bad type\n";
        exit(1);
    }
    return p->value;
}

class Task;

template<typename T>
struct TaskWrapper {
    std::shared_ptr<Task> task;
};

class Task {
private:
    struct TaskModel {
        virtual void Execute(Any& result) = 0;

        virtual TaskModel* GetCopy() const = 0;

        virtual ~TaskModel() = default;

        template <typename Arg>
        const Arg& GetValue(const Arg& arg) {
            return arg;
        }

        template <typename ResultType>
        const ResultType& GetValue(const TaskWrapper<ResultType>& task_wrapper) {
            return task_wrapper.task->template GetResult<ResultType>();
        }
    };

    template <typename Function>
    struct TaskConceptZero: public TaskModel {
        Function function;

        TaskConceptZero(const Function& function): function(function) {}

        TaskModel* GetCopy() const override {
            return new TaskConceptZero(function);
        }

        void Execute(Any& result) override {
            result = function();
        }
    };

    template <typename Function, typename Arg>
    struct TaskConceptOne: public TaskModel {
        Function function;
        Arg arg;

        TaskConceptOne(const Function& function, const Arg& arg): function(function), arg(arg) {}

        TaskModel* GetCopy() const override {
            return new TaskConceptOne(function, arg);
        }

        void Execute(Any& result) override {
            result = function(GetValue(arg));
        }
    };

    template <typename Function, typename Arg1, typename Arg2>
    struct TaskConceptTwo: public TaskModel {
        Function function;
        Arg1 arg1;
        Arg2 arg2;

        TaskConceptTwo(const Function& function, const Arg1& arg1, const Arg2& arg2):
                function(function), arg1(arg1), arg2(arg2) {}

        TaskModel* GetCopy() const override {
            return new TaskConceptTwo(function, arg1, arg2);
        }

        void Execute(Any& result) override {
            result = function(GetValue(arg1), GetValue(arg2));
        }
    };

    Any result_;
    TaskModel* ptr_;

public:
    template <typename Function, typename Arg1, typename Arg2>
    Task(const Function& function, const Arg1& arg1, const Arg2& arg2): ptr_(new TaskConceptTwo(function, arg1, arg2)) {}

    template <typename Function, typename Arg>
    Task(const Function& function, const Arg& arg): ptr_(new TaskConceptOne(function, arg)) {}

    template <typename Function>
    Task(const Function& function): ptr_(new TaskConceptZero(function)) {}

    template <typename ResultType>
    const ResultType& GetResult() {
        if (!result_.HasValue()) {
            Execute();
        }

        return GetValue<ResultType>(result_);
    }

    void Execute() {
        ptr_->Execute(result_);
    }

    Task(const Task& other): ptr_(other.ptr_->GetCopy()), result_(other.result_) {}

    ~Task() {
        delete ptr_;
    }
};

class Scheduler {
public:
    template <typename Function, typename Arg1, typename Arg2>
    std::shared_ptr<Task> add(const Function& function, const Arg1& arg1, const Arg2& arg2) {
        tasks.push_back(std::make_shared<Task>(function, arg1, arg2));
        return tasks.back();
    }

    template <typename Function, typename Arg>
    std::shared_ptr<Task> add(const Function& function, const Arg& arg) {
        tasks.push_back(std::make_shared<Task>(function, arg));
        return tasks.back();
    }

    template <typename Function>
    std::shared_ptr<Task> add(const Function& function) {
        tasks.push_back(std::make_shared<Task>(function));
        return tasks.back();
    }

    template <typename ResultType>
    TaskWrapper<ResultType> GetFutureResult(std::shared_ptr<Task> task) {
        return {task};
    }

    template <typename ResultType>
    const ResultType& GetResult(const std::shared_ptr<Task>& task) {
        return task->GetResult<ResultType>();
    }

    void ExecuteAll() {
        for (int i = 0; i < tasks.size(); ++i) {
            tasks[i]->Execute();
        }
    }

    ~Scheduler() = default;

    Scheduler() = default;

    Scheduler(const Scheduler& other) {
        tasks.resize(other.tasks.size());
        for (int i = 0; i < other.tasks.size(); ++i) {
            tasks[i] = other.tasks[i];
        }
    }

private:
    std::vector<std::shared_ptr<Task>> tasks;
};