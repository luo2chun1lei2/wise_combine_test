extern "C" int init() {
    return 0;
}

extern "C" int start() {
    return 0;
}

extern "C" int finish() {
    return 0;
}

extern "C" int fail() {
    // 预置问题：组合流程一旦触发失败路径，就返回非零。
    return 1;
}

extern "C" int release() {
    return 0;
}
