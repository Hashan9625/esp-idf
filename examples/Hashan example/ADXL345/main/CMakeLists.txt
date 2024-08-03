# Edit following two lines to set component requirements (see docs)

set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "main.c" "wifi.c" "i2c.c" "ina226.c" "adxl345.c")
set(COMPONENT_EMBED_TXTFILES "AmazonRootCA1.pem" "certificate.pem.crt" "private.pem.key")
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()