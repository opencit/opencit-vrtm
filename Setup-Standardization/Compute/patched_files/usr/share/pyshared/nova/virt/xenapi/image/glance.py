# Copyright 2013 OpenStack Foundation
# All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.

from oslo.config import cfg

from nova import exception
from nova.image import glance
from nova import utils
from nova.virt.xenapi import vm_utils

CONF = cfg.CONF
CONF.import_opt('glance_num_retries', 'nova.image.glance')


class GlanceStore(object):
    def _call_glance_plugin(self, session, fn, params):
        glance_api_servers = glance.get_api_servers()

        def pick_glance(kwargs):
            g_host, g_port, g_use_ssl = glance_api_servers.next()
            kwargs['glance_host'] = g_host
            kwargs['glance_port'] = g_port
            kwargs['glance_use_ssl'] = g_use_ssl

        return session.call_plugin_serialized_with_retry(
            'glance', fn, CONF.glance_num_retries, pick_glance, **params)

    def _make_params(self, context, session, image_id):
        return {'image_id': image_id,
                'sr_path': vm_utils.get_sr_path(session),
                'extra_headers': glance.generate_identity_headers(context)}

    """ MH start of download_manifest_file"""
    def download_manifest_file(self, context, session, instance, image_id, vdis):
        params = self._make_params(context, session, image_id)
        params['vdis'] = vdis

        try:
            self._call_glance_plugin(session, 'download_manifest_file', params)
        except exception.PluginRetriesExceeded:
            raise exception.CouldNotFetchImage(image_id=image_id)

    """ MH end of download_manifest_file"""

    def download_image(self, context, session, instance, image_id):
        params = self._make_params(context, session, image_id)
        params['uuid_stack'] = vm_utils._make_uuid_stack()
       
        # jbuhacoff adding mh properties to params XXX can be rewriten without conditional as getattr(instance,varname,None or 'false')
        # jbuhacoff adding fix for missing extra_headers parameter, and putting the auth_token there so glance client can use it

        if 'mh_encrypted' in instance:
            params['mh_encrypted'] = instance['mh_encrypted']
            params['mh_checksum'] = instance['mh_checksum']
            #params['mh_dek_url'] = instance['mh_dek_url']
        else:
            params['mh_encrypted'] = 'false'
            params['mh_checksum'] = None
            #params['mh_dek_url'] = None
        params['auth_token'] = getattr(context, 'auth_token', None)

        # jbuhacoff end

        try:
            vdis = self._call_glance_plugin(session, 'download_vhd', params)
        except exception.PluginRetriesExceeded:
            raise exception.CouldNotFetchImage(image_id=image_id)

        return vdis

    def upload_image(self, context, session, instance, vdi_uuids, image_id):
        params = self._make_params(context, session, image_id)
        params['vdi_uuids'] = vdi_uuids

        props = params['properties'] = {}
        props['auto_disk_config'] = instance['auto_disk_config']
        props['os_type'] = instance['os_type'] or CONF.default_os_type

        compression_level = vm_utils.get_compression_level()
        if compression_level:
            props['xenapi_image_compression_level'] = compression_level

        auto_disk_config = utils.get_auto_disk_config_from_instance(instance)
        if utils.is_auto_disk_config_disabled(auto_disk_config):
            props["auto_disk_config"] = "disabled"

        try:
            self._call_glance_plugin(session, 'upload_vhd', params)
        except exception.PluginRetriesExceeded:
            raise exception.CouldNotUploadImage(image_id=image_id)
