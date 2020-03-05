<?php

function sample_prof_start(int $interval_usec = 1, int $entries_alloc = 1 << 20) {}

function sample_prof_end(): bool {}

function sample_prof_get_data(): array {}
