// Copyright (c) 2022 Friedt Professional Engineering Services, Inc
// SPDX-License-Identifier: MIT

service Edu {
    void ping();
    string echo(1: string msg);
    i32 counter();
}