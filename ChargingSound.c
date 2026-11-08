#include <AudioToolbox/AudioServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <pthread.h>
#include <string.h>

#if defined(THEOS_PACKAGE_SCHEME_ROOTHIDE)
#include <roothide.h>
#endif

// The C entry point from Substrate and compatible jailbreak injectors.
extern void MSHookFunction(void *function, void *replacement, void **original);

static const SystemSoundID ChargingSound = 1106;
#define REPLACEMENT_PATH "/Library/Application Support/iOS6ChargingSound/connect_power.caf"

typedef void (^SoundCompletion)(void);
static OSStatus (*originalCreate)(CFURLRef, SystemSoundID *);
static void (*originalPlay)(SystemSoundID);
static void (*originalPlayWithCompletion)(SystemSoundID, SoundCompletion);
static void (*originalPlayWithOptions)(SystemSoundID, CFDictionaryRef, SoundCompletion);

static CFURLRef CopyReplacementURL(void) {
#if defined(THEOS_PACKAGE_SCHEME_ROOTHIDE)
    const char *path = jbroot(REPLACEMENT_PATH);
#elif defined(THEOS_PACKAGE_SCHEME_ROOTLESS)
    const char *path = "/var/jb" REPLACEMENT_PATH;
#else
    const char *path = REPLACEMENT_PATH;
#endif
    if (!path) return NULL;
    return CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                                    (const UInt8 *)path,
                                                    strlen(path), false);
}

static OSStatus CreateReplacementSound(SystemSoundID *sound) {
    CFURLRef url = CopyReplacementURL();
    if (!url) return kAudioServicesSystemSoundUnspecifiedError;
    OSStatus result = originalCreate(url, sound);
    CFRelease(url);
    return result;
}

static OSStatus CreateSound(CFURLRef url, SystemSoundID *sound) {
    if (url && sound) {
        CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
        if (path) {
            Boolean charging = CFEqual(path, CFSTR("/System/Library/Audio/UISounds/connect_power.aif")) ||
                               CFEqual(path, CFSTR("/System/Library/Audio/UISounds/connect_power.caf"));
            CFRelease(path);
            if (charging) {
                SystemSoundID replacement = 0;
                if (CreateReplacementSound(&replacement) == noErr && replacement != 0) {
                    *sound = replacement;
                    return noErr;
                }
            }
        }
    }
    return originalCreate(url, sound);
}

static SystemSoundID ReplaceChargingSound(SystemSoundID sound) {
    if (sound != ChargingSound) return sound;

    // Keep one ID for the life of SpringBoard. Retry if registration fails.
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static SystemSoundID replacement;
    pthread_mutex_lock(&lock);
    if (!replacement) {
        SystemSoundID candidate = 0;
        if (CreateReplacementSound(&candidate) == noErr) replacement = candidate;
    }
    SystemSoundID selected = replacement ? replacement : sound;
    pthread_mutex_unlock(&lock);
    return selected;
}

static void Play(SystemSoundID sound) {
    originalPlay(ReplaceChargingSound(sound));
}

static void PlayWithCompletion(SystemSoundID sound, SoundCompletion completion) {
    originalPlayWithCompletion(ReplaceChargingSound(sound), completion);
}

static void PlayWithOptions(SystemSoundID sound, CFDictionaryRef options, SoundCompletion completion) {
    originalPlayWithOptions(ReplaceChargingSound(sound), options, completion);
}

static void InstallHook(const char *name, void *replacement, void **original) {
    void *symbol = dlsym(RTLD_DEFAULT, name);
    if (symbol) MSHookFunction(symbol, replacement, original);
}

__attribute__((constructor)) static void InstallChargingSound(void) {
    InstallHook("AudioServicesCreateSystemSoundID", (void *)CreateSound, (void **)&originalCreate);
    if (!originalCreate) return;
    InstallHook("AudioServicesPlaySystemSound", (void *)Play, (void **)&originalPlay);
    InstallHook("AudioServicesPlaySystemSoundWithCompletion", (void *)PlayWithCompletion,
                (void **)&originalPlayWithCompletion);
    InstallHook("AudioServicesPlaySystemSoundWithOptions", (void *)PlayWithOptions,
                (void **)&originalPlayWithOptions);
}
