BEGIN { FS = "\t" }
NR == 1 { next }
{
    wall[++n] = $2 + 0; user[n] = $3 + 0; sys[n] = $4 + 0; rss[n] = $5 + 0
}
function sort(a, count, i, j, v) {
    for (i = 2; i <= count; i++) {
        v = a[i]; j = i - 1
        while (j >= 1 && a[j] > v) { a[j + 1] = a[j]; j-- }
        a[j + 1] = v
    }
}
function emit(name, a, count, median) {
    sort(a, count)
    median = count % 2 ? a[(count + 1) / 2] : (a[count / 2] + a[count / 2 + 1]) / 2
    printf "%s\t%.6f\t%.6f\t%.6f\t%.6f\n", name, median, a[1], a[count], a[count] - a[1]
}
END {
    if (n < 1) exit 2
    print "metric\tmedian\tmin\tmax\trange"
    emit("wall_seconds", wall, n)
    emit("user_seconds", user, n)
    emit("sys_seconds", sys, n)
    emit("max_rss_kb", rss, n)
}
