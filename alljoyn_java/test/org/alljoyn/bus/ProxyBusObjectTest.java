/**
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

package org.alljoyn.bus;

import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

import junit.framework.TestCase;

import org.alljoyn.bus.annotation.BusInterface;
import org.alljoyn.bus.annotation.BusProperty;
import org.alljoyn.bus.ifaces.DBusProxyObj;

public class ProxyBusObjectTest extends TestCase {
    public ProxyBusObjectTest(String name) {
        super(name);
    }

    static {
        System.loadLibrary("alljoyn_java");
    }

    private int uniquifier = 0;
    private String genUniqueName(BusAttachment bus) {
        return "test.x" + bus.getGlobalGUIDString() + ".x" + uniquifier++;
    }

    private String name;
    private BusAttachment otherBus;
    private Service service;
    private BusAttachment bus;
    private ProxyBusObject proxyObj;

    public void setUp() throws Exception {
        otherBus = new BusAttachment(getClass().getName(), BusAttachment.RemoteMessage.Receive);
        name = genUniqueName(otherBus);
        service = new Service();
        assertEquals(Status.OK, otherBus.registerBusObject(service, "/simple"));
        assertEquals(Status.OK, otherBus.connect());

        bus = new BusAttachment(getClass().getName(), BusAttachment.RemoteMessage.Receive);
        assertEquals(Status.OK, bus.connect());
        while (bus.getDBusProxyObj().NameHasOwner(name)) {
            Thread.sleep(100);
        }

        assertEquals(Status.OK, otherBus.requestName(name, DBusProxyObj.REQUEST_NAME_NO_FLAGS));
    }

    public void tearDown() throws Exception {
        bus.disconnect();

        otherBus.releaseName(name);
        otherBus.unregisterBusObject(service);
        otherBus.disconnect();

        bus.release();
        otherBus.release();
    }

    public class DelayReply implements SimpleInterface,
                                       BusObject {
        public String ping(String str) {
            boolean thrown = false;
            try {
                Thread.sleep(100);
            } catch (InterruptedException ex) {
                // we don't expect the sleep to be interrupted. If it is print
                // the stacktrace to aid with debugging.
                ex.printStackTrace();
                thrown = true;
            }
            assertFalse(thrown);
            return str;
        }
    }

    public void testReplyTimeout() throws Exception {
        DelayReply service = new DelayReply();
        assertEquals(Status.OK, otherBus.registerBusObject(service, "/delayreply"));

        proxyObj = bus.getProxyBusObject(name, "/delayreply", BusAttachment.SESSION_ID_ANY, new Class<?>[] { SimpleInterface.class });
        proxyObj.setReplyTimeout(10);

        boolean thrown = false;
        try {
            SimpleInterface simple = proxyObj.getInterface(SimpleInterface.class);
            simple.ping("testReplyTimeout");
        } catch (ErrorReplyBusException ex) {
            thrown = true;
        }
        assertEquals(true, thrown);
    }

    public class Service implements SimpleInterface, BusObject {
        public String ping(String inStr) { return inStr; }
    }

    public void testCreateRelease() throws Exception {
        proxyObj = bus.getProxyBusObject(name, "/simple", BusAttachment.SESSION_ID_ANY, new Class<?>[] { SimpleInterface.class });
        proxyObj.release();
    }


    public void testMethodCall() throws Exception {
        assertEquals(Status.OK, otherBus.advertiseName(name, SessionOpts.TRANSPORT_ANY));

        proxyObj = bus.getProxyBusObject(name, "/simple", BusAttachment.SESSION_ID_ANY, new Class<?>[] { SimpleInterface.class });
        SimpleInterface proxy = proxyObj.getInterface(SimpleInterface.class);
        for (int i = 0; i < 10; ++i) {
            assertEquals("ping", proxy.ping("ping"));
        }

        proxyObj.release();

        otherBus.cancelAdvertiseName(name, SessionOpts.TRANSPORT_ANY);
    }

    public void testMultipleProxyBusObjects() throws Exception {
        // Connect two proxy objects
        proxyObj = bus.getProxyBusObject(name, "/simple", BusAttachment.SESSION_ID_ANY, new Class<?>[] { SimpleInterface.class });
        ProxyBusObject proxyObj2 = bus.getProxyBusObject(name, "/simple", BusAttachment.SESSION_PORT_ANY, new Class<?>[] { SimpleInterface.class });

        // Verify they're both operating
        assertEquals("ping", proxyObj.getInterface(SimpleInterface.class).ping("ping"));
        assertEquals("ping2", proxyObj2.getInterface(SimpleInterface.class).ping("ping2"));

        // release one of them
        proxyObj2.release();

        // Verify the other is still working
        assertEquals("ping", proxyObj.getInterface(SimpleInterface.class).ping("ping"));

        // Disconnect other one
        proxyObj.release();
    }

    public class Emitter implements EmitterInterface,
                                    BusObject {
        public void emit(String str) { /* Do nothing, this is a signal. */ }
    }

    /* Call a @BusSignal on a ProxyBusObject interface. */
    public void testSignalFromInterface() throws Exception {
        Emitter service = new Emitter();
        assertEquals(Status.OK, otherBus.registerBusObject(service, "/emitter"));

        boolean thrown = false;
        try {
            proxyObj = bus.getProxyBusObject(name, "/emitter", BusAttachment.SESSION_ID_ANY, new Class<?>[] { EmitterInterface.class });
            proxyObj.getInterface(EmitterInterface.class).emit("emit");
        } catch (BusException ex) {
            thrown = true;
        }
        assertTrue(thrown);

        proxyObj.release();
    }

    private boolean methods;
    private boolean methodi;

    public class MultiMethod implements MultiMethodInterfaceA, MultiMethodInterfaceB,
                                        BusObject {
        public void method(String str) { methods = true; };
        public void method(int i) { methodi = true; };
    };

    public void testMultiMethod() throws Exception {
        MultiMethod service = new MultiMethod();
        assertEquals(Status.OK, otherBus.registerBusObject(service, "/multimethod"));

        proxyObj = bus.getProxyBusObject(name, "/multimethod", BusAttachment.SESSION_ID_ANY,
                                         new Class<?>[] { MultiMethodInterfaceA.class, MultiMethodInterfaceB.class });

        try {
            methods = methodi = false;
            MultiMethodInterfaceA ifacA = proxyObj.getInterface(MultiMethodInterfaceA.class);
            ifacA.method("str");
            assertEquals(true, methods);
            assertEquals(false, methodi);

            methods = methodi = false;
            proxyObj.getInterface(MultiMethodInterfaceB.class).method(10);
            assertEquals(false, methods);
            assertEquals(true, methodi);
        } catch(BusException ex) {
            /*
             * This catch statement should not be run if it is run print out a
             * stack trace and fail the unit test.
             */
            ex.printStackTrace();
            System.out.println(ex.getMessage());
            assertTrue(false);
        }
        proxyObj.release();
    }

    public void testGetBusName() throws Exception {
        proxyObj = bus.getProxyBusObject(name, "/simple", BusAttachment.SESSION_ID_ANY, new Class<?>[] { SimpleInterface.class });
        assertEquals(name, proxyObj.getBusName());
        proxyObj.release();
    }

    public void testGetObjPath() throws Exception {
        proxyObj = bus.getProxyBusObject(name, "/simple", BusAttachment.SESSION_ID_ANY, new Class<?>[] { SimpleInterface.class });
        assertEquals("/simple", proxyObj.getObjPath());
        proxyObj.release();
    }

    public void testProxiedMethodsFromClassObject() throws Exception {
        proxyObj = bus.getProxyBusObject(name, "/simple",
                BusAttachment.SESSION_ID_ANY,
                new Class<?>[] { SimpleInterface.class });
        Object intfObject = proxyObj.getInterface(SimpleInterface.class);
        assertTrue(intfObject.equals(intfObject));
        assertFalse(intfObject.equals(null));
        assertFalse(intfObject.equals(this));

        assertNotNull(intfObject.toString());

        assertEquals(System.identityHashCode(intfObject), intfObject.hashCode());

        proxyObj.release();
    }

    @BusInterface(name = BasicProperty.PROP_INTF_NAME)
    static interface BasicProperty {
        String PROP_INTF_NAME = "my.property.test";
        @BusProperty(annotation = BusProperty.ANNOTATE_EMIT_CHANGED_SIGNAL)
        String getName();
    }

    class PropertyObject implements BasicProperty, BusObject {
        private String name = "MyName";
        final AtomicInteger callCount = new AtomicInteger(0);
	@Override
        public String getName() {
            callCount.incrementAndGet();
            return name;
        }
    }

    public void testPropertyCache() throws Exception {
        final String propertyServicePath = "/myProperties";
        PropertyObject ps = new PropertyObject();

        assertEquals(Status.OK, otherBus.registerBusObject(ps, propertyServicePath));
        proxyObj = bus.getProxyBusObject(name, propertyServicePath,
                BusAttachment.SESSION_ID_ANY,
                new Class<?>[] { BasicProperty.class });
        assertNotNull(proxyObj);
        final BasicProperty pp = proxyObj.getInterface(BasicProperty.class);
        assertNotNull(pp);
        assertEquals(ps.name, pp.getName());
        assertEquals(1, ps.callCount.get());
        proxyObj.enablePropertyCaching();
        /* wait until the property cache is enabled. We have no reliable way to check this,
         * so we'll just wait for a sufficiently long time period. */
        Thread.sleep(500);
        assertEquals(ps.name, pp.getName());
        assertEquals(2, ps.callCount.get());
        assertEquals(ps.name, pp.getName());
        assertEquals(2, ps.callCount.get());
        proxyObj.enablePropertyCaching();
        proxyObj.enablePropertyCaching();
        proxyObj.enablePropertyCaching();
        assertEquals(ps.name, pp.getName());
        assertEquals(2, ps.callCount.get());

        String oldName = ps.name;
        ps.name = "newName";
        assertEquals(oldName, pp.getName());

        /* updates to the property should be reflected in the cache.
         * The update must be available during the property change callback. */
        synchronized (this) {
            final ArrayList<String> values = new ArrayList<String>();
            proxyObj.registerPropertiesChangedListener(BasicProperty.PROP_INTF_NAME, new String[]{"Name"}, new PropertiesChangedListener() {
                @Override
                public void propertiesChanged(ProxyBusObject pObj, String ifaceName,
                    Map<String, Variant> changed, String[] invalidated) {

                    values.add(pp.getName());
                    synchronized (ProxyBusObjectTest.this) {
                        ProxyBusObjectTest.this.notifyAll();
                    }
                }
            });
            PropertyChangedEmitter pce = new PropertyChangedEmitter(ps);
            pce.PropertyChanged(BasicProperty.PROP_INTF_NAME, "Name", new Variant(ps.name));
            wait(1500);
            assertEquals(1, values.size());
            assertEquals(ps.name, values.get(0));
            assertEquals(ps.name, pp.getName());
            assertEquals(2, ps.callCount.get());
        }
        otherBus.unregisterBusObject(ps);
        /* The property cache should still provide the last known value
         * even after the remote objects is unregistered. */
        assertEquals(ps.name, pp.getName());
        assertEquals(ps.name, pp.getName());
        assertEquals(ps.name, pp.getName());
        proxyObj.release();
    }
}
