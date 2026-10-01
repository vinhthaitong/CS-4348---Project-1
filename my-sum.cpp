#define _POSIX_C_SOURCE 200809L

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <new>
#include <signal.h>
#include <string>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

void reportError(const char *operation)
{
    int errorNumber = errno;
    cerr << "my-sum: " << operation << ": " << strerror(errorNumber) << '\n';
}

bool readPositiveInt(const char *text, int &value)
{
    char *end;
    errno = 0;
    long number = strtol(text, &end, 10);

    if (errno == ERANGE || end == text || *end != '\0' || number <= 0 || number > INT_MAX)
        return false;

    value = static_cast<int>(number);
    return true;
}

// volatile makes every loop check reread flags that other workers may change.
void arriveAndWait(volatile unsigned char *wall, int worker, int m)
{
    wall[worker] = 1;

    for (int i = 0; i < m; i++)
        while (wall[i] == 0)
            ;
}

void compute(long long *rows, volatile unsigned char *walls, int n, int m, int worker, int rounds)
{
    int base = n / m;
    int extra = n % m;
    int start = worker * base + (worker < extra ? worker : extra);
    int end = start + base + (worker < extra ? 1 : 0);
    int offset = 1;

    for (int round = 0; round < rounds; round++) {
        long long *current = rows + round * n;
        long long *next = current + n;

        for (int i = start; i < end; i++) {
            if (i < offset)
                next[i] = current[i];
            else
                next[i] = current[i] + current[i - offset];
        }

        arriveAndWait(walls + round * m, worker, m);

        if (round + 1 < rounds)
            offset *= 2;
    }
}

bool waitForWorkers(pid_t *children, int count)
{
    int finished = 0;

    while (finished < count) {
        int status;

        // Wait for any worker to finish and store its exit information in status.
        pid_t pid = waitpid(-1, &status, 0);

        if (pid < 0) {
            if (errno == EINTR)
                continue;
            reportError("waitpid");
            return false;
        }

        for (int i = 0; i < count; i++) {
            if (children[i] == pid) {
                children[i] = 0;
                break;
            }
        }

        finished++;

        // A worker failed if it ended abnormally or returned a nonzero exit status.
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            cerr << "my-sum: worker " << pid << " failed\n";
            return false;
        }
    }

    return true;
}

void stopWorkers(pid_t *children, int count)
{
    for (int i = 0; i < count; i++) {
        if (children[i] != 0 &&
            kill(children[i], SIGTERM) < 0 && errno != ESRCH)
            reportError("kill");
    }

    for (int i = 0; i < count; i++) {
        if (children[i] == 0)
            continue;

        int status;

        // Wait for the stopped worker so it does not remain a zombie process.
        while (waitpid(children[i], &status, 0) < 0) {
            if (errno == EINTR)
                continue;
            if (errno != ECHILD)
                reportError("waitpid during cleanup");
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 5) {
        cerr << "Usage: ./my-sum n m input_file output_file\n";
        return 1;
    }

    int n, m;
    if (!readPositiveInt(argv[1], n) || !readPositiveInt(argv[2], m) || m > n) {
        cerr << "my-sum: n and m must be positive integers with 1 <= m <= n\n";
        return 1;
    }

    int rounds = 0;
    for (int remaining = n - 1; remaining > 0; remaining /= 2)
        rounds++;

    // size_t is the standard type for memory sizes and byte counts.
    size_t rowBytes = static_cast<size_t>(n) * (rounds + 1) * sizeof(long long);
    size_t wallBytes = static_cast<size_t>(rounds) * m;

    pid_t *children = new (nothrow) pid_t[m];
    if (children == nullptr) {
        cerr << "my-sum: cannot allocate child PID array\n";
        return 1;
    }

    // Create one shared-memory segment for all rows and barrier flags.
    int memoryId = shmget(IPC_PRIVATE, rowBytes + wallBytes, IPC_CREAT | 0600);
    if (memoryId < 0) {
        reportError("shmget");
        delete[] children;
        return 1;
    }

    // Attach the shared-memory segment and receive a usable memory address.
    void *memory = shmat(memoryId, nullptr, 0);
    if (memory == reinterpret_cast<void *>(-1)) {
        reportError("shmat");

        // Remove the segment because attaching it failed.
        if (shmctl(memoryId, IPC_RMID, nullptr) < 0)
            reportError("shmctl");
        delete[] children;
        return 1;
    }

    long long *rows = static_cast<long long *>(memory);

    // volatile makes workers reread barrier flags changed by other workers.
    volatile unsigned char *walls = static_cast<unsigned char *>(memory) + rowBytes;

    bool success = true;
    ifstream input(argv[3]);
    if (!input) {
        cerr << "my-sum: cannot open input file: " << argv[3] << '\n';
        success = false;
    }

    for (int i = 0; success && i < n; i++) {
        string token;
        if (!(input >> token)) {
            cerr << "my-sum: cannot read integer " << i + 1 << '\n';
            success = false;
            break;
        }

        char *end;
        errno = 0;

        // Convert the token's base-10 text to long long; end marks where conversion stopped.
        long long value = strtoll(token.c_str(), &end, 10);

        // Reject values that are too large, contain no number, or have extra characters.
        if (errno == ERANGE || end == token.c_str() || *end != '\0') {
            cerr << "my-sum: invalid integer at position " << i + 1 << '\n';
            success = false;
            break;
        }
        rows[i] = value;
    }

    int created = 0;
    for (int worker = 0; success && worker < m; worker++) {
        // Create a child worker: 0 means child, positive means parent, -1 means error.
        pid_t pid = fork();
        if (pid < 0) {
            reportError("fork");
            success = false;
        } else if (pid == 0) {
            compute(rows, walls, n, m, worker, rounds);
            bool ok = true;

            // Disconnect this child process from shared memory.
            if (shmdt(memory) < 0) {
                reportError("shmdt");
                ok = false;
            }
            _exit(ok ? 0 : 1);
        } else {
            children[created++] = pid;
        }
    }

    if (success)
        success = waitForWorkers(children, created);
    if (!success)
        stopWorkers(children, created);

    if (success) {
        ofstream output(argv[4]);
        if (!output) {
            cerr << "my-sum: cannot open output file: " << argv[4] << '\n';
            success = false;
        } else {
            long long *answer = rows + rounds * n;
            for (int i = 0; i < n; i++) {
                output << answer[i] << (i + 1 == n ? '\n' : ' ');
                if (!output) {
                    cerr << "my-sum: failed to write output file\n";
                    success = false;
                    break;
                }
            }

            output.close();
            if (output.fail()) {
                cerr << "my-sum: failed to close output file\n";
                success = false;
            }
        }
    }

    // Disconnect the parent process from shared memory.
    if (shmdt(memory) < 0) {
        reportError("shmdt");
        success = false;
    }

    // Mark the shared-memory segment for removal by the operating system.
    if (shmctl(memoryId, IPC_RMID, nullptr) < 0) {
        reportError("shmctl");
        success = false;
    }

    delete[] children;

    return success ? 0 : 1;
}
