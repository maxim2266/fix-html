# builder
FROM alpine:latest AS builder
RUN		[ "apk", "add", "--no-cache", "gcc", "make", "git", "musl-dev", "gumbo-parser-dev", "gumbo-parser-static" ]
WORKDIR	/build
COPY	. .
RUN		[ "make", "clean", "static" ]

# result
FROM	scratch
COPY	--from=builder /build/fix-html /bin/fix-html
