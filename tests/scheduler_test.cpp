#include <lib/scheduler.cpp>
#include <gtest/gtest.h>
#include <valarray>

class AnyStorage {
public:
    template <typename F, typename A1, typename A2>
    AnyStorage(F f, A1 a1, A2 a2): f(f), a1(a1), a2(a2) {}

    template<typename T>
    const T& Get(int number) const {
        switch (number) {
            case 0:
                return GetValue<T>(f);
            case 1:
                return GetValue<T>(a1);
            case 2:
                return GetValue<T>(a2);
        }
        exit(1);
    }

private:
    Any f;
    Any a1;
    Any a2;
};

TEST(schedulerTestSuite, AnyStorageTest) {
    int number = 1;
    const char* string = "some_string";
    std::vector<int> vector = {1, 2, 3};

    AnyStorage any_storage_r(1, "some_string", std::vector<int> {1, 2, 3});
    ASSERT_EQ(number, any_storage_r.Get<int>(0));
    ASSERT_EQ(string, any_storage_r.Get<const char*>(1));
    ASSERT_EQ(vector, any_storage_r.Get<std::vector<int>>(2));

    AnyStorage any_storage_l(number, string, vector);
    ASSERT_EQ(number, any_storage_l.Get<int>(0));
    ASSERT_EQ(string, any_storage_l.Get<const char*>(1));
    ASSERT_EQ(vector, any_storage_l.Get<std::vector<int>>(2));
}

TEST(schedulerTestSuite, AnyCopyTest) {
    using ivector = std::vector<int>;
    ivector v1 = {1, 2, 3, 4, 5, 123};
    ivector v3 = v1;
    ivector v2 = {1, 2, 3, 4, 5, 321};

    Any any_1 = ivector {1, 2, 3, 4, 5};
    Any any_2 = any_1;
    GetValue<ivector>(any_1).push_back(123);
    Any any_3 = any_2;
    any_3 = any_1;
    any_3 = any_3;
    GetValue<ivector>(any_2).push_back(321);

    ASSERT_EQ(v1, GetValue<ivector>(any_1));
    ASSERT_EQ(v2, GetValue<ivector>(any_2));
    ASSERT_EQ(v3, GetValue<ivector>(any_3));

    Any int_any = 3;
    any_3 = int_any;
    any_3 = any_3;
    ASSERT_EQ(3, GetValue<int>(any_3));
}

TEST(schedulerTestSuite, CommonTestTwoArgs) {
    float a = 1;
    float b = -2;
    float c = 0;

    Scheduler scheduler;

    auto id1 = scheduler.add([](float a, float c){return -4 * a * c;}, a, c);

    auto id2 = scheduler.add([](float b, float v){return b * b + v;}, b, scheduler.GetFutureResult<float>(id1));

    auto id3 = scheduler.add([](float b, float d){return -b + std::sqrt(d);}, b, scheduler.GetFutureResult<float>(id2));

    auto id4 = scheduler.add([](float b, float d){return -b - std::sqrt(d);}, b, scheduler.GetFutureResult<float>(id2));

    auto id5 = scheduler.add([](float a, float v) {return v/(2*a);}, a, scheduler.GetFutureResult<float>(id3));

    auto id6 = scheduler.add([](float a, float v) {return v/(2*a);},a, scheduler.GetFutureResult<float>(id4));

    scheduler.ExecuteAll();

    ASSERT_EQ(2, scheduler.GetResult<float>(id5));
    ASSERT_EQ(0, scheduler.GetResult<float>(id6));
}

TEST(schedulerTestSuite, MemoryLickTest) {
    for (int i = 0; i < 10e5; ++i) {
        float a = 1;
        float b = -2;
        float c = 0;

        Scheduler scheduler, sch2, sch3;

        auto id1 = scheduler.add([](float a, float c){return -4 * a * c;}, a, c);
        auto id2 = scheduler.add([](float b, float v){return b * b + v;},
                                                    b, scheduler.GetFutureResult<float>(id1));
        auto id3 = scheduler.add([](float b, float d){return -b + std::sqrt(d);},
                                                    b, scheduler.GetFutureResult<float>(id2));

        Scheduler sch4 = scheduler;

        auto id4 = scheduler.add([](float b, float d){return -b - std::sqrt(d);},
                                                    b, scheduler.GetFutureResult<float>(id2));

        auto id5 = scheduler.add([](float a, float v) {return v/(2*a);},
                                                    a, scheduler.GetFutureResult<float>(id3));
        auto id6 = scheduler.add([](float a, float v) {return v/(2*a);},
                                                    a, scheduler.GetFutureResult<float>(id4));
        scheduler.ExecuteAll();
    }
}

struct A {
    int key = 0;
    std::string name;
};

struct B {
    double key = 0.0;
    std::string name;
    A a;
};

TEST(schedulerTestSuite, StructsTest) {
    Scheduler sh;

    // ReturnType - double(2.0)
    auto node1 = sh.add([](A a, B b) { return a.key + b.key; },
                A(1, "Vikluchatel"), B(1, "Justic", A(0, "")));

    // ReturnType - const char*("Huligan")
    auto node2 = sh.add([]() { return "Huligan"; });

    // ReturnType - A(2.0, "Huligan")
    auto node3 = sh.add([](double key, const std::string& name) { return A(key, name); },
                        sh.GetFutureResult<double>(node1), sh.GetFutureResult<const char*>(node2));

    // ReturnType - B (0, "Btype", {2.0, "Huligan"})
    auto node4 = sh.add([](const A& a) { return B(0, "Btype", a); },sh.GetFutureResult<A>(node3));

    Scheduler cloned = sh;

    // ReturnType - int (123)
    auto cloned_node = cloned.add([]() { return 123; });

    Scheduler cloned_twice = cloned;

    // ReturnType - int (223)
    auto cloned_by_operator_node = cloned_twice.add([](int x) { return x + 100; }, cloned.GetFutureResult<int>(cloned_node));

    sh.ExecuteAll();
    cloned.ExecuteAll();
    cloned_twice.ExecuteAll();

    ASSERT_EQ(sh.GetResult<B>(node4).key, 0.0);
    ASSERT_EQ(sh.GetResult<B>(node4).name, "Btype");
    ASSERT_EQ(sh.GetResult<B>(node4).a.key, 2);
    ASSERT_EQ(sh.GetResult<B>(node4).a.name, "Huligan");
    ASSERT_EQ(cloned.GetResult<int>(cloned_node), 123);
    ASSERT_EQ(cloned_twice.GetResult<int>(cloned_by_operator_node), 223);
}

struct func1 {
    float operator()(float a, float c) const {
        return -4 * a * c;
    }
};

float func11(float a, float c) {
    return func1{}(a, c);
}

struct func2 {
    float operator()(float b, float v) const {
        return b * b + v;
    }
};

float func22(float b, float v) {
    return func2{}(b, v);
}

struct func3 {
    auto operator()(float b, float d) const {
        return -b + std::sqrt(d);
    }
};

float func33(float b, float d) {
    return func3{}(b, d);
}

struct func4 {
    float operator()(float a, float v) const {
        return v/(2*a);
    }
};

float func44(float a, float v) {
    return func4{}(a, v);
}


TEST(schedulerTestSuite, CommonTestTwoArgsFunctors) {
    float a = 1;
    float b = -2;
    float c = 0;

    Scheduler scheduler;

    auto id1 = scheduler.add(func1{}, a, c);

    auto id2 = scheduler.add(func2{}, b, scheduler.GetFutureResult<float>(id1));

    auto id3 = scheduler.add(func3{}, b, scheduler.GetFutureResult<float>(id2));

    auto id4 = scheduler.add([](float b, float d){return -b - std::sqrt(d);},
                                            b, scheduler.GetFutureResult<float>(id2));

    auto id5 = scheduler.add(func4{}, a, scheduler.GetFutureResult<float>(id3));

    auto id6 = scheduler.add(func4{},a, scheduler.GetFutureResult<float>(id4));

    scheduler.ExecuteAll();

    ASSERT_EQ(2, scheduler.GetResult<float>(id5));
    ASSERT_EQ(0, scheduler.GetResult<float>(id6));
}


TEST(schedulerTestSuite, CommonTestTwoArgsFunctions) {
    float a = 1;
    float b = -2;
    float c = 0;

    Scheduler scheduler;

    auto id1 = scheduler.add(&func11, a, c);

    auto id2 = scheduler.add(func2{}, b, scheduler.GetFutureResult<float>(id1));

    auto id3 = scheduler.add(&func33, b, scheduler.GetFutureResult<float>(id2));

    auto id4 = scheduler.add([](float b, float d){return -b - std::sqrt(d);},
                                                b, scheduler.GetFutureResult<float>(id2));

    auto id5 = scheduler.add(&func44, a, scheduler.GetFutureResult<float>(id3));

    auto id6 = scheduler.add(func4{},a, scheduler.GetFutureResult<float>(id4));

    scheduler.ExecuteAll();

    ASSERT_EQ(2, scheduler.GetResult<float>(id5));
    ASSERT_EQ(0, scheduler.GetResult<float>(id6));
}


TEST(schedulerTestSuite, DependenceSchedulersTest1) {
    Scheduler scheduler;
    auto t1 = scheduler.add([](){ return 1; });

    Scheduler scheduler2;
    auto t2 = scheduler2.add([](auto t, auto scheduler)
            { return 2 + scheduler.template GetResult<int>(t); }, t1, scheduler);

    scheduler2.ExecuteAll();
    ASSERT_EQ(scheduler2.GetResult<int>(t2), 3);
}

Scheduler MakeScheduler() {
    float a = 1;
    float b = -2;
    float c = 0;

    Scheduler scheduler;

    auto id1 = scheduler.add([](float a, float c){return -4 * a * c;}, a, c);

    auto id2 = scheduler.add([](float b, float v){return b * b + v;}, b, scheduler.GetFutureResult<float>(id1));

    auto id3 = scheduler.add([](float b, float d){return -b + std::sqrt(d);}, b, scheduler.GetFutureResult<float>(id2));

    auto id4 = scheduler.add([](float b, float d){return -b - std::sqrt(d);}, b, scheduler.GetFutureResult<float>(id2));

    auto id5 = scheduler.add([](float a, float v) {return v/(2*a);}, a, scheduler.GetFutureResult<float>(id3));

    auto id6 = scheduler.add([](float a, float v) {return v/(2*a);},a, scheduler.GetFutureResult<float>(id4));

    Scheduler scheduler2 = scheduler;
    return scheduler2;
}

TEST(schedulerTestSuite, DependenceSchedulersTest2) {
    Scheduler scheduler = MakeScheduler();
    scheduler.ExecuteAll();
}

TEST(schedulerTestSuite, DependenceSchedulersTest3) {
    Scheduler scheduler;
    auto t1 = scheduler.add([](){ return 1; });
    auto t2 = scheduler.add([](auto t1){ return 2; }, t1);
    auto t3 = scheduler.add([](auto t2){ return 3; }, t2);

    Scheduler scheduler1 = scheduler;
    Scheduler scheduler2 = scheduler;

    auto res1 = scheduler1.add([](int x, int y) {return x * y;},
                   scheduler.GetFutureResult<int>(t1), scheduler.GetFutureResult<int>(t2));
    auto res2 = scheduler2.add([](int x, int y) {return x * x * y * y;},
                   scheduler.GetFutureResult<int>(t2), scheduler.GetFutureResult<int>(t3));

    ASSERT_EQ(scheduler1.GetResult<int>(res1), 2);
    ASSERT_EQ(scheduler2.GetResult<int>(res2), 36);
    ASSERT_EQ(scheduler1.GetResult<int>(t1), 1);
    ASSERT_EQ(scheduler1.GetResult<int>(t2), 2);
    ASSERT_EQ(scheduler1.GetResult<int>(t3), 3);
}

