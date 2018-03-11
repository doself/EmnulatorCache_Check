package cache.faker;

import android.util.Log;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.content.Intent;
import android.content.Context;
import android.widget.TextView;
import android.os.RemoteException;
import android.content.ComponentName;
import android.content.ServiceConnection;
import android.support.v7.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    public TextView tv;
    public static String test_result = "";
    private Messenger serviceMessenger; //定义一个和Service通讯的Messenger
    private static final int recv_code = 100;
    private static final int send_code = 101;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        tv = findViewById(R.id.sample_text);
        bindService();
    }

    //定义一个和handler绑定的Messenger
    private Messenger messenger = new Messenger(new Handler() {
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            switch (msg.what) {
                case recv_code:
                    Bundle receive = msg.getData();
                    test_result = (String)receive.get("reply");
                    tv.setText(test_result);
                    break;
            }
        }
    });

    //绑定远程service
    private void bindService() {
        Intent intent = new Intent();
        //指定要绑定的service
        intent.setClassName(getApplicationContext(), "cache.faker.DetectService");
        //绑定服务
        bindService(intent, conn, Context.BIND_AUTO_CREATE);
    }

    ServiceConnection conn = new ServiceConnection() {

        @Override
        public void onServiceDisconnected(ComponentName name) {
            // TODO Auto-generated method stub
            test_result = "detect process is crash!";
            Log.d("emulator_check",test_result);
            tv.setText(test_result);
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            // TODO Auto-generated method stub
            //当和远程service建立连接后，通过IBinder service我们可以建立和远程service互动的Messenger
            //IBinder的service对象是绑定的service中onBind()方法返回的对象
            serviceMessenger = new Messenger(service);
            Message msg = new Message();
            msg.what = send_code;
            //远端利用设置的replyTo的messenger发送消息
            msg.replyTo = messenger;
            try {
                serviceMessenger.send(msg);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    };

}
