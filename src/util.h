#pragma once

void fail(const char *fmt, ...);

void runcmd(const char* cmd);

char* getcmdoutput(const char* command);
