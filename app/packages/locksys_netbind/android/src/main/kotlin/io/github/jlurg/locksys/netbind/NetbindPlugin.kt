// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

package io.github.jlurg.locksys.netbind

import android.app.Activity
import android.content.Context
import android.net.wifi.WifiManager
import android.os.Build
import android.view.WindowManager
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding

/**
 * Android implementation of [NetbindHostApi].
 *
 * M0 skeleton: capabilities and the secure-window flag are implemented; join and probe report
 * [JoinStatus.ERROR] / [LocalNetStatus.ERROR] until the WifiNetworkSpecifier join lands (M4).
 */
class NetbindPlugin : FlutterPlugin, ActivityAware, NetbindHostApi {
    private var context: Context? = null
    private var activity: Activity? = null
    private val events = NetEventsHandler()

    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        context = binding.applicationContext
        NetbindHostApi.setUp(binding.binaryMessenger, this)
        NetEventsStreamHandler.register(binding.binaryMessenger, events)
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        NetbindHostApi.setUp(binding.binaryMessenger, null)
        context = null
    }

    override fun onAttachedToActivity(binding: ActivityPluginBinding) {
        activity = binding.activity
    }

    override fun onDetachedFromActivityForConfigChanges() {
        activity = null
    }

    override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {
        activity = binding.activity
    }

    override fun onDetachedFromActivity() {
        activity = null
    }

    override fun capabilities(): NetCapabilities {
        val wifi = context?.applicationContext?.getSystemService(Context.WIFI_SERVICE) as? WifiManager
        val sae = wifi?.isWpa3SaeSupported ?: false
        val staConcurrency =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                wifi?.isStaConcurrencyForLocalOnlyConnectionsSupported ?: false
            } else {
                false
            }
        return NetCapabilities(
            osVersion = Build.VERSION.SDK_INT.toString(),
            wpa3Sae = sae,
            staConcurrencyLocalOnly = staConcurrency,
        )
    }

    override suspend fun join(request: JoinRequest): JoinResult =
        JoinResult(status = JoinStatus.ERROR, detail = "join is not implemented in this build")

    override suspend fun probe(host: String, port: Long, timeoutMs: Long): LocalNetResult =
        LocalNetResult(status = LocalNetStatus.ERROR)

    override fun release() {
        // No network request is held by this build.
    }

    override fun setSecureWindow(enabled: Boolean) {
        val window = activity?.window ?: return
        if (enabled) {
            window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
        }
    }

    /** Event stream of network changes; this build emits no events. */
    private class NetEventsHandler : NetEventsStreamHandler()
}
