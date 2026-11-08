THEOS_PACKAGE_SCHEME ?= rootless

ARCHS = arm64 arm64e
TARGET = iphone:clang:latest:15.0

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = iOS6ChargingSound
iOS6ChargingSound_FILES = ChargingSound.c
iOS6ChargingSound_FRAMEWORKS = AudioToolbox CoreFoundation
iOS6ChargingSound_LIBRARIES = substrate
iOS6ChargingSound_CFLAGS = -Wall -Wextra -Werror -fblocks
ifeq ($(THEOS_PACKAGE_SCHEME),roothide)
iOS6ChargingSound_LIBRARIES += roothide
endif

include $(THEOS_MAKE_PATH)/tweak.mk
