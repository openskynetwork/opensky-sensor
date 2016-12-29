#pragma once

/** GPS Status, as provided by rcd */
typedef enum {
	/** Timestamp is invalid */
	GpsTimeInvalid,
	/** Using CPU time */
	UsingCpuTime,
	/** Using GPS time */
	UsingGpsTime
} GpsTimeStatus_t;
