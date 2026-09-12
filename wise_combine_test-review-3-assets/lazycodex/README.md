# LazyCodex independent review assets

These assets map the frozen Q1-Q6 queue oracle to the LazyCodex JSON model. Each
scenario is a linear state graph whose transition `expect.state` is the raw
return observation (`r0`, `r11`, and so on). `queue_adapter.cpp` is a thin
translator only: it calls the queue function, emits the raw return as
`observed_state`, and persists `QueueState` in a private file because the native
runner launches one adapter process per step. The adapter contains no expected
value assertions. The state file is deleted before every run.
