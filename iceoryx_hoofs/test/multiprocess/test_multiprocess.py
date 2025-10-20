# content of test_sysexit.py
import time
import signal
import subprocess


def close_processes(processes):
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

    EXPECTED_PUBERR_NUM = 1
    if len(puberr) != EXPECTED_PUBERR_NUM:
        close_processes([subscriber, roudi])
    assert len(puberr) == EXPECTED_PUBERR_NUM

    EXPECTED_PUBOUT_NUM = 2
    if len(pubout) != EXPECTED_PUBOUT_NUM:
        close_processes([subscriber, roudi])
    assert len(pubout) == EXPECTED_PUBOUT_NUM

    pubaddr = pubout[-1].split(" ")[-1]

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

    EXPECTED_SUBERR_NUM = 1
    if len(suberr) != EXPECTED_SUBERR_NUM:
        close_processes([roudi])
    assert len(suberr) == EXPECTED_SUBERR_NUM

    EXPECTED_SUBOUT_NUM = 2
    if len(subout) != EXPECTED_SUBOUT_NUM:
        close_processes([roudi])
    assert len(subout) == EXPECTED_SUBOUT_NUM

    subaddr = subout[-1].split(" ")[-1]

    roudi.send_signal(2)

    assert subaddr == pubaddr
