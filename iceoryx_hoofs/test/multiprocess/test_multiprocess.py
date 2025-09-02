# content of test_sysexit.py
import time
import signal
import subprocess

def close_processes(processes: list[subprocess.Popen]):
    for process in processes:
        poll = process.poll()
        if poll is None:
            process.send_signal(signal.SIGINT)


def test_address():
    print("Run roudi")
    roudi = subprocess.Popen(
        "./iox-roudi",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    time.sleep(1)

    print("Run subscriber")
    subscriber = subprocess.Popen(
        "./hoofs/test/multiprocess-test-subscriber",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    time.sleep(1)

    print("Run publisher")
    publisher = subprocess.Popen(
        "./hoofs/test/multiprocess-test-publisher",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    print("Wait publisher")
    pubcode = publisher.wait()
    if pubcode != 0:
        close_processes([subscriber, roudi])
    assert pubcode == 0

    puberr = [
        line for line in publisher.stderr.read().decode("utf-8").split("\n") if line
    ]
    pubout = [
        line for line in publisher.stdout.read().decode("utf-8").split("\n") if line
    ]
    print(f"pub stderr: {puberr}")
    print(f"pub stdout: {pubout}")

    if len(puberr) != 1:
        close_processes([subscriber, roudi])
    assert len(puberr) == 1

    if len(pubout) != 1:
        close_processes([subscriber, roudi])
    assert len(pubout) == 1

    pubaddr = pubout[0].split(" ")[-1]

    subcode = 0
    try:
        subcode = subscriber.wait(timeout=5)
    except subprocess.TimeoutExpired:
        roudi.send_signal(2)
        raise

    if subcode != 0:
        close_processes([roudi])
    assert subcode == 0

    suberr = [
        line for line in subscriber.stderr.read().decode("utf-8").split("\n") if line
    ]
    subout = [
        line for line in subscriber.stdout.read().decode("utf-8").split("\n") if line
    ]
    print(f"sub stderr: {suberr}")
    print(f"sub stdout: {subout}")

    if len(suberr) != 1:
        close_processes([roudi])
    assert len(suberr) == 1

    if len(subout) != 1:
        close_processes([roudi])
    assert len(subout) == 1

    subaddr = subout[0].split(" ")[-1]

    roudi.send_signal(2)

    assert subaddr == pubaddr
