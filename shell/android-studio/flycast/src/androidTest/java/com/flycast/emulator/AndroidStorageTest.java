package com.flycast.emulator;

import android.util.Log;

import androidx.test.core.app.ActivityScenario;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

import static org.junit.Assert.*;

@RunWith(AndroidJUnit4.class)
public class AndroidStorageTest {
    public static final String TREE_URI = "content://com.android.externalstorage.documents/tree/primary%3AFlycast%2FROMS";

    @Test
    public void test() {
        ActivityScenario<NativeGLActivity> scenario = ActivityScenario.launch(NativeGLActivity.class);
        scenario.onActivity(activity -> {
            try {
                // Configure storage
                AndroidStorage storage = activity.getStorage();
                String rootUri = TREE_URI;
                storage.setStorageDirectories(Arrays.asList(rootUri));

                // Start test
                // exists (root)
                assertTrue(storage.exists(rootUri));
                // listContent (root)
                FileInfo[] kids = storage.listContent(rootUri);
                assertTrue(kids.length > 0);
                // getFileInfo (root)
                FileInfo info = storage.getFileInfo(rootUri);
                assertEquals(info.getPath(), rootUri);
                assertTrue(info.isDirectory());
                assertNotEquals(0, info.getUpdateTime());
                // getParentUri (root)
                // fails on lollipop_mr1, could be because parent folder (/Flycast) is also allowed
                assertEquals("", storage.getParentUri(rootUri));

                boolean directoryDone = false;
                boolean fileDone = false;
                for (FileInfo file : kids) {
                    if (file.isDirectory() && !directoryDone) {
                        // getParentUri
                        String parentUri = storage.getParentUri(file.getPath());
                        // FIXME fails because getParentUri returns a docId, not a treeId
                        //assertEquals(rootUri, parentUri);

                        // getSubPath (from root)
                        String kidUri = storage.getSubPath(rootUri, file.getName());
                        assertEquals(file.getPath(), kidUri);

                        // exists (folder)
                        assertTrue(storage.exists(file.getPath()));

                        // getFileInfo (folder)
                        info = storage.getFileInfo(file.getPath());
                        assertEquals(file.getPath(), info.getPath());
                        assertEquals(file.getName(), info.getName());
                        assertTrue(info.isDirectory());
                        assertNotEquals(0, info.getUpdateTime());
                        assertTrue(info.isDirectory());

                        // listContent (from folder)
                        FileInfo[] gdkids = storage.listContent(file.getPath());
                        assertTrue(gdkids.length > 0);
                        for (FileInfo sfile : gdkids) {
                            if (!sfile.isDirectory()) {
                                // openFile
                                int fd = storage.openFile(sfile.getPath(), "r");
                                assertNotEquals(-1, fd);
                                // getSubPath (from folder)
                                String uri = storage.getSubPath(file.getPath(), sfile.getName());
                                assertEquals(sfile.getPath(), uri);
                                // getParentUri (from file)
                                uri = storage.getParentUri(sfile.getPath());
                                assertEquals(file.getPath(), uri);
                                // exists (doc)
                                assertTrue(storage.exists(sfile.getPath()));
                                // getFileInfo (doc)
                                info = storage.getFileInfo(sfile.getPath());
                                assertEquals(info.getPath(), sfile.getPath());
                                assertEquals(info.getName(), sfile.getName());
                                assertEquals(info.isDirectory(), sfile.isDirectory());
                                assertNotEquals(0, info.getUpdateTime());
                                assertFalse(info.isDirectory());
                            } else {
                                // getParentUri (from subfolder)
                                String uri = storage.getParentUri(sfile.getPath());
                                assertEquals(file.getPath(), uri);
                                // exists (subfolder)
                                assertTrue(storage.exists(sfile.getPath()));
                            }
                        }
                        directoryDone = true;
                    }
                    if (!file.isDirectory() && !fileDone) {
                        // getParentUri
                        String parentUri = storage.getParentUri(file.getPath());
                        // FIXME fails because getParentUri returns a docId, not a treeId
                        //assertEquals(rootUri, parentUri);
                        // getSubPath (from root)
                        String kidUri = storage.getSubPath(rootUri, file.getName());
                        assertEquals(file.getPath(), kidUri);
                        // exists (file)
                        assertTrue(storage.exists(file.getPath()));
                        fileDone = true;
                    }
                }
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        });
    }

    //@Test
    public void testLargeFolder() {
        ActivityScenario<NativeGLActivity> scenario = ActivityScenario.launch(NativeGLActivity.class);
        scenario.onActivity(activity -> {
            try {
                // Configure storage
                AndroidStorage storage = activity.getStorage();
                String rootUri = TREE_URI;
                storage.setStorageDirectories(Arrays.asList(rootUri));

                // list content
                String uri = storage.getSubPath(rootUri, "textures");
                uri = storage.getSubPath(uri, "T1401N");
                long t0 = System.currentTimeMillis();
                FileInfo[] kids = storage.listContent(uri);
                Log.d("testLargeFolder", "Got " + kids.length + " in " + (System.currentTimeMillis() - t0) + " ms");
                // Got 2307 in 119910 ms !!!
                // retrieving only uri in listContent: Got 2307 in 9007 ms
                // retrieving uri+isDir: Got 2307 in 62281 ms
                // manual listing: Got 2307 in 10212 ms
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        });
    }
}