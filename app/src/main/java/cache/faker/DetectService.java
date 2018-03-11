package cache.faker;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

/**
 * Created by wireghost on 2018/3/3.
 */

public class DetectService extends Service {

    private static final int send_code = 100;
    private static final int recv_code = 101;

    static {
        System.loadLibrary("emulator_check");
    }

    public static native int detect();

    public static String isEmulator_rate() {
        return "" + detect();
    }

    private Handler remoteHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            switch (msg.what) {
                case recv_code:
                    try {
                        Messenger client = msg.replyTo;
                        Message replyMessage = Message.obtain(null,send_code);
                        Bundle bundle = new Bundle();
                        bundle.putString("reply",isEmulator_rate());
                        replyMessage.setData(bundle);
                        client.send(replyMessage);
                    } catch (RemoteException e) {
                        e.printStackTrace();
                    }
                    break;
            }
        }
    };

    private Messenger messenger = new Messenger(remoteHandler);

    @Override
    public IBinder onBind(Intent intent) {
        //返回给客户端的IBinder
        return messenger.getBinder();
    }

    @Override
    public void onCreate() {
        super.onCreate();
    }
}
