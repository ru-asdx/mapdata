FROM alpine:3.15 as builder

WORKDIR /tmp
COPY . .

RUN apk add --no-cache git gcc musl-dev && \
    gcc mapdata22.c -g -o mapdata && \
    strip --strip-all ./mapdata

##

FROM alpine:3.15
MAINTAINER asdx

RUN apk add --update tini; \
    rm -rf /var/cache/apk/* ; \
    adduser -S -H map

WORKDIR /app

COPY --from=builder /tmp/mapdata ./

USER map

ENTRYPOINT ["/sbin/tini", "--"]
CMD ["/app/mapdata"]
