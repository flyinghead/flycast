/*
	Copyright 2023 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
package com.flycast.emulator;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ContentUris;
import android.content.CursorLoader;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.MediaStore;

import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.List;

public class AndroidStorage {
    public static final int ADD_STORAGE_ACTIVITY_REQUEST = 15012010;

    private Activity activity;

    private List<String> storageDirectories;
    private int storageIntentPerms;

    public AndroidStorage(Activity activity) {
        this.activity = activity;
        init();
    }

    public void setStorageDirectories(List<String> storageDirectories) {
        this.storageDirectories = storageDirectories;
    }

    public native void init();
    public native void addStorageCallback(String path);

    public void onAddStorageResult(Intent data) {
        Uri uri = data == null ? null : data.getData();
        if (uri == null) {
            // Cancelled
            addStorageCallback(null);
        }
        else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
                activity.getContentResolver().takePersistableUriPermission(uri, storageIntentPerms);
            String realPath = getRealPath(uri);
            if (realPath != null)
                addStorageCallback(realPath);
            else
                addStorageCallback(uri.toString());
        }
    }

    public int openFile(String uri, String mode) throws FileNotFoundException {
        ParcelFileDescriptor pfd = activity.getContentResolver().openFileDescriptor(Uri.parse(uri), mode);
        return pfd.detachFd();
    }

    public FileInfo[] listContent(String uri) {
        DocumentFile docFile = DocumentFile.fromTreeUri(activity, Uri.parse(uri));
        DocumentFile kids[] = docFile.listFiles();
        FileInfo ret[] = new FileInfo[kids.length];
        for (int i = 0; i < kids.length; i++) {
            ret[i] = new FileInfo();
            ret[i].setName(kids[i].getName());
            ret[i].setPath(kids[i].getUri().toString());
            ret[i].setDirectory(kids[i].isDirectory());
        }
        return ret;
    }

    public String getParentUri(String uriString) throws FileNotFoundException {
        if (uriString.isEmpty())
            return "";
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                Uri uri = Uri.parse(uriString);
                DocumentsContract.Path path = DocumentsContract.findDocumentPath(activity.getContentResolver(), uri);
                List<String> comps = path.getPath();
                if (comps.size() > 1)
                    return DocumentsContract.buildDocumentUriUsingTree(uri, comps.get(comps.size() - 2)).toString();
                else
                    return "";
            } catch (IllegalArgumentException e) {
                // Happens for root storage uri:
                // DocumentsContract: Failed to find path: Invalid URI: content://com.android.externalstorage.documents/tree/primary%3AFlycast
                return "";
            }
        }
        // Hack the uri manually
        int i = uriString.lastIndexOf("%2F");
        if (i == -1)
            return "";
        else
            return uriString.substring(0, i);
    }

    public String getSubPath(String reference, String relative) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            Uri refUri = Uri.parse(reference);
            String docId = DocumentsContract.getDocumentId(refUri);
            String ret;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
                ret = DocumentsContract.buildDocumentUriUsingTree(refUri, docId + "/" + relative).toString();
            else
                ret = DocumentsContract.buildDocumentUri(refUri.getAuthority(), docId + "/" + relative).toString();
            return ret;
        }
        else {
            throw new UnsupportedOperationException("getSubPath unsupported");
        }
    }

    public FileInfo getFileInfo(String uriString) throws FileNotFoundException {
        Uri uri = Uri.parse(uriString);
        DocumentFile docFile = DocumentFile.fromTreeUri(activity, uri);
        if (!docFile.exists())
            throw new FileNotFoundException(uriString);
        FileInfo info = new FileInfo();
        info.setPath(uriString);
        info.setName(docFile.getName());
        info.setDirectory(docFile.isDirectory());
        info.setSize(docFile.length());
        info.setWritable(docFile.canWrite());

        return info;
    }

    public void addStorage(boolean isDirectory, boolean writeAccess) {
        Intent intent = new Intent(isDirectory ? Intent.ACTION_OPEN_DOCUMENT_TREE : Intent.ACTION_OPEN_DOCUMENT);
        if (!isDirectory) {
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/*");
            intent = Intent.createChooser(intent, "Select a cheat file");
        }
        else {
            intent = Intent.createChooser(intent, "Select a directory");
        }
        storageIntentPerms = Intent.FLAG_GRANT_READ_URI_PERMISSION | (writeAccess ? Intent.FLAG_GRANT_WRITE_URI_PERMISSION : 0);
        intent.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION | storageIntentPerms);
        activity.startActivityForResult(intent, ADD_STORAGE_ACTIVITY_REQUEST);
    }

    private String getRealPath(final Uri uri) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
            return getRealPathFromURI_API19(uri);
        else
            return getRealPathFromURI_BelowAPI19(uri);
    }

    // From https://github.com/HBiSoft/PickiT
    // Copyright (c) [2020] [HBiSoft]
    @SuppressLint("NewApi")
    String getRealPathFromURI_API19(final Uri uri)
    {
        final boolean isKitKat = Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT;

        if (isKitKat && (DocumentsContract.isDocumentUri(activity, uri) || DocumentsContract.isTreeUri(uri))) {
            final boolean isTree = DocumentsContract.isTreeUri(uri);
            if (isExternalStorageDocument(uri)) {
                final String docId = isTree ? DocumentsContract.getTreeDocumentId(uri) : DocumentsContract.getDocumentId(uri);
                final String[] split = docId.split(":");
                final String type = split[0];

                if ("primary".equalsIgnoreCase(type)) {
                    if (split.length > 1)
                        return Environment.getExternalStorageDirectory() + "/" + split[1];
                    else
                        return Environment.getExternalStorageDirectory() + "/";
                }
                else {
                    // Some devices does not allow access to the SD Card using the UID, for example /storage/6551-1152/folder/video.mp4
                    // Instead, we first have to get the name of the SD Card, for example /storage/sdcard1/folder/video.mp4

                    // We first have to check if the device allows this access
                    if (new File("storage" + "/" + docId.replace(":", "/")).exists()) {
                        return "/storage/" + docId.replace(":", "/");
                    }
                    if (storageDirectories != null) {
                        // If the file is not available, look at the storage directories
                        for (String s : storageDirectories) {
                            String root;
                            if (split[1].startsWith("/")) {
                                root = s + split[1];
                            } else {
                                root = s + "/" + split[1];
                            }
                            String path;
                            if (root.contains(type)) {
                                path = "storage" + "/" + docId.replace(":", "/");
                            } else {
                                if (root.startsWith("/storage/") || root.startsWith("storage/")) {
                                    path = root;
                                } else if (root.startsWith("/")) {
                                    path = "/storage" + root;
                                } else {
                                    path = "/storage/" + root;
                                }
                            }
                            if (new File(path).exists())
                                return path;
                        }
                    }
                }

            }
            else if (!isTree && isRawDownloadsDocument(uri)){
                String fileName = getFilePath(uri);
                String subFolderName = getSubFolders(uri);

                if (fileName != null) {
                    return Environment.getExternalStorageDirectory().toString() + "/Download/"+subFolderName + fileName;
                }
                String id = DocumentsContract.getDocumentId(uri);

                final Uri contentUri = ContentUris.withAppendedId(Uri.parse("content://downloads/public_downloads"), Long.valueOf(id));
                return getDataColumn(contentUri, null, null);
            }
            else if (!isTree && isDownloadsDocument(uri)) {
                String fileName = getFilePath(uri);

                if (fileName != null) {
                    return Environment.getExternalStorageDirectory().toString() + "/Download/"+ fileName;
                }
                String id = DocumentsContract.getDocumentId(uri);
                if (id.startsWith("raw:")) {
                    id = id.replaceFirst("raw:", "");
                    File file = new File(id);
                    if (file.exists())
                        return id;
                }
                if (id.startsWith("raw%3A%2F")){
                    id = id.replaceFirst("raw%3A%2F", "");
                    File file = new File(id);
                    if (file.exists())
                        return id;
                }
                final Uri contentUri = ContentUris.withAppendedId(Uri.parse("content://downloads/public_downloads"), Long.valueOf(id));
                return getDataColumn(contentUri, null, null);
            }
            else if (!isTree && isMediaDocument(uri)) {
                final String docId = DocumentsContract.getDocumentId(uri);
                final String[] split = docId.split(":");
                final String type = split[0];

                Uri contentUri = null;
                if ("image".equals(type)) {
                    contentUri = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
                } else if ("video".equals(type)) {
                    contentUri = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
                } else if ("audio".equals(type)) {
                    contentUri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
                }

                final String selection = "_id=?";
                final String[] selectionArgs = new String[]{
                        split[1]
                };

                return getDataColumn(contentUri, selection, selectionArgs);
            }
        }
        else if ("content".equalsIgnoreCase(uri.getScheme())) {
            if (isGooglePhotosUri(uri))
                return uri.getLastPathSegment();
            else
                return getDataColumn(uri, null, null);
        }
        else if ("file".equalsIgnoreCase(uri.getScheme())) {
            return uri.getPath();
        }

        return null;
    }

    private static String getSubFolders(Uri uri) {
        String replaceChars = String.valueOf(uri).replace("%2F", "/").replace("%20", " ").replace("%3A",":");
        String[] bits = replaceChars.split("/");
        String sub5 = bits[bits.length - 2];
        String sub4 = bits[bits.length - 3];
        String sub3 = bits[bits.length - 4];
        String sub2 = bits[bits.length - 5];
        String sub1 = bits[bits.length - 6];
        if (sub1.equals("Download")){
            return sub2+"/"+sub3+"/"+sub4+"/"+sub5+"/";
        }
        else if (sub2.equals("Download")){
            return sub3+"/"+sub4+"/"+sub5+"/";
        }
        else if (sub3.equals("Download")){
            return sub4+"/"+sub5+"/";
        }
        else if (sub4.equals("Download")){
            return sub5+"/";
        }
        else {
            return "";
        }
    }

    private String getDataColumn(Uri uri, String selection, String[] selectionArgs) {
        Cursor cursor = null;
        final String column = "_data";
        final String[] projection = {column};
        try {
            cursor = activity.getContentResolver().query(uri, projection, selection, selectionArgs, null);
            if (cursor != null && cursor.moveToFirst()) {
                final int index = cursor.getColumnIndexOrThrow(column);
                return cursor.getString(index);
            }
        } catch (Exception e) {
        } finally {
            if (cursor != null)
                cursor.close();
        }
        return null;
    }

    private String getFilePath(Uri uri) {
        Cursor cursor = null;
        final String[] projection = {MediaStore.Files.FileColumns.DISPLAY_NAME};
        try {
            cursor = activity.getContentResolver().query(uri, projection, null, null,
                    null);
            if (cursor != null && cursor.moveToFirst()) {
                final int index = cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns.DISPLAY_NAME);
                return cursor.getString(index);
            }
        } catch (Exception e) {
        } finally {
            if (cursor != null)
                cursor.close();
        }
        return null;
    }

    private static boolean isExternalStorageDocument(Uri uri) {
        return "com.android.externalstorage.documents".equals(uri.getAuthority());
    }

    private static boolean isDownloadsDocument(Uri uri) {
        return "com.android.providers.downloads.documents".equals(uri.getAuthority());
    }

    private static boolean isRawDownloadsDocument(Uri uri) {
        String uriToString = String.valueOf(uri);
        return uriToString.contains("com.android.providers.downloads.documents/document/raw");
    }

    private static boolean isMediaDocument(Uri uri) {
        return "com.android.providers.media.documents".equals(uri.getAuthority());
    }

    private static boolean isGooglePhotosUri(Uri uri) {
        return "com.google.android.apps.photos.content".equals(uri.getAuthority());
    }

    String getRealPathFromURI_BelowAPI19(Uri contentUri) {
        String[] proj = {MediaStore.Video.Media.DATA};
        CursorLoader loader = new CursorLoader(activity, contentUri, proj, null, null, null);
        Cursor cursor = loader.loadInBackground();
        int column_index = cursor.getColumnIndexOrThrow(MediaStore.Video.Media.DATA);
        cursor.moveToFirst();
        String result = cursor.getString(column_index);
        cursor.close();
        return result;
    }
}
