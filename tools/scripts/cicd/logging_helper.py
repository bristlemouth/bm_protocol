import logging
import git
from pathlib import Path
from time import time
import os
import sys


class LoggingHelper:

    LOG_DIR = "sys_logs"
    CRITICAL = 50
    ERROR = 40
    WARNING = 30
    INFO = 20
    DEBUG = 10

    def __init__(self, sub_dir: str, name: str, stdout: bool = False):
        repo = git.Repo(search_parent_directories=True)
        commit_hash = repo.head.commit.hexsha
        home = Path.home()
        directory = str(home) + "/" + self.LOG_DIR + "/" + sub_dir
        os.makedirs(directory, mode=0o777, exist_ok=True)
        filename = (
            directory
            + "/"
            + name
            + "_"
            + commit_hash[:7]
            + "_"
            + str(int(time()))
            + ".log"
        )

        # Set Up Logger to file
        self.log_name = name
        self.logger = logging.getLogger(self.log_name)
        self.logger.setLevel(logging.DEBUG)
        fh = logging.FileHandler(filename)
        fh.setLevel(logging.DEBUG)

        # Configure formatter
        formatter = logging.Formatter("%(asctime)s - %(levelname)s: %(message)s")
        fh.setFormatter(formatter)

        # Add file handler
        self.logger.addHandler(fh)

        # Add to stdout if required
        if stdout is True:
            ch = logging.StreamHandler(sys.stdout)
            ch.setLevel(logging.DEBUG)
            ch.setFormatter(formatter)
            self.logger.addHandler(ch)

    def log(self, level: int = DEBUG, msg: str = None):
        msg = msg.strip()
        if level == self.CRITICAL:
            self.logger.critical(msg)
        elif level == self.ERROR:
            self.logger.error(msg)
        elif level == self.WARNING:
            self.logger.warning(msg)
        elif level == self.INFO:
            self.logger.info(msg)
        else:
            self.logger.debug(msg)
